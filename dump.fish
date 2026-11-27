#!/usr/bin/env fish

set -g WD_BUILD_DIR "programs/bin-x86_64"
set -g WD_SHELL_BIN "programs/bin-x86_64/system/dogeshell"
set -g WD_KERNEL_ELF "kernel.elf"

function wd_help
    echo "Usage: windoge_debug.fish <command> [args...]"
    echo ""
    echo "Commands:"
    echo "  bss <elf_or_obj> [top_n]       List largest .bss symbols and total .bss range"
    echo "  addr <elf_or_obj> <hex_addr>   Find symbols/functions near a virtual address (e.g. 0x421000)"
    echo "  elf <elf_file>                 Show section headers, program headers, and .bss end address"
    echo "  range <elf_file> <start> <end> List symbols in an ELF falling inside [start, end)"
    echo "  log <logfile>                  Highlight #PF, CR2, FS_WRITE, SYSCALL, and ELF lines"
    echo "  faults <logfile>               Extract Page Fault blocks with CR2, RSP, CR3, and callers"
    echo "  qemu [qemu_args...]            Run QEMU with serial/debug output piped through fault highlighter"
    echo "  all [target_prog]              Run quick ELF + .bss + 0x421000 check on shell & target"
end

function wd_resolve_bin
    set -l target $argv[1]
    if test -e "$target"
        echo "$target"
        return 0
    end
    if test -e "$WD_BUILD_DIR/$target"
        echo "$WD_BUILD_DIR/$target"
        return 0
    end
    if test -e "$WD_BUILD_DIR/$target.o"
        echo "$WD_BUILD_DIR/$target.o"
        return 0
    end
    echo "Error: cannot find binary or object '$target'" >&2
    return 1
end

function wd_elf_info
    set -l bin (wd_resolve_bin $argv[1])
    or return 1

    echo "=== ELF: $bin ==="
    readelf -l -S "$bin"

    set -l bss_line (readelf -S -W "$bin" | grep -E '\s\.bss\s' | head -n 1)
    if test -n "$bss_line"
        set -l parts (string split -n -r ' ' (string replace -ra '\s+' ' ' (string trim "$bss_line")))
        # Typical readelf -S columns: [Nr] Name Type Addr Off Size ...
        set -l hex_addr (readelf -S -W "$bin" | awk '/\.bss/ {for(i=1;i<=NF;i++) if($i=="NOBITS" || $i=="PROGBITS") {print $(i+1); exit}}')
        set -l hex_size (readelf -S -W "$bin" | awk '/\.bss/ {for(i=1;i<=NF;i++) if($i=="NOBITS" || $i=="PROGBITS") {print $(i+3); exit}}')
        if test -n "$hex_addr" -a -n "$hex_size"
            set -l end_addr (math --base=hex "0x$hex_addr + 0x$hex_size")
            set -l dec_size (math "0x$hex_size")
            echo ""
            echo ".bss start : 0x$hex_addr"
            echo ".bss size  : 0x$hex_size ($dec_size bytes)"
            echo ".bss end   : $end_addr"
        end
    end
end

function wd_bss_symbols
    set -l bin (wd_resolve_bin $argv[1])
    or return 1
    set -l top_n 25
    if test (count $argv) -ge 2
        set top_n $argv[2]
    end

    echo "=== Largest .bss (b/B) symbols in $bin (top $top_n) ==="
    nm --print-size --size-sort -r "$bin" 2>/dev/null | grep -E ' [bB] ' | head -n "$top_n"
    or begin
        echo "Fallback (no size-sort match, showing all .bss symbols):"
        nm -n "$bin" 2>/dev/null | grep -E ' [bB] '
    end
end

function wd_addr_lookup
    if test (count $argv) -lt 2
        echo "Usage: wd_addr_lookup <elf_or_obj> <hex_addr>" >&2
        return 1
    end
    set -l bin (wd_resolve_bin $argv[1])
    or return 1
    set -l raw_addr $argv[2]
    set -l norm_addr (string lower (string replace -r '^0x' '' "$raw_addr"))
    set -l target_dec (math "0x$norm_addr")

    echo "=== Symbols near 0x$norm_addr in $bin ==="
    nm -n --print-size "$bin" 2>/dev/null | awk -v t="$target_dec" '
        {
            addr_hex = $1
            gsub(/^0+/, "", addr_hex)
            if (addr_hex == "") addr_hex = "0"
            addr_dec = strtonum("0x" addr_hex)
            diff = addr_dec - t
            abs_diff = (diff < 0) ? -diff : diff
            if (abs_diff <= 0x2000) {
                printf "%s  (delta %+d bytes / %+x)\n", $0, diff, diff
            }
        }
    '

    if command -q addr2line
        echo ""
        echo "=== addr2line (0x$norm_addr) ==="
        addr2line -e "$bin" -f -p "0x$norm_addr" 2>/dev/null
    end
end

function wd_range_symbols
    if test (count $argv) -lt 3
        echo "Usage: wd_range_symbols <elf_file> <start_hex> <end_hex>" >&2
        return 1
    end
    set -l bin (wd_resolve_bin $argv[1])
    or return 1
    set -l start_dec (math "0x"(string lower (string replace -r '^0x' '' "$argv[2]")))
    set -l end_dec   (math "0x"(string lower (string replace -r '^0x' '' "$argv[3]")))

    echo "=== Symbols in [$argv[2], $argv[3]) for $bin ==="
    nm -n --print-size "$bin" 2>/dev/null | awk -v s="$start_dec" -v e="$end_dec" '
        {
            addr_hex = $1
            gsub(/^0+/, "", addr_hex)
            if (addr_hex == "") addr_hex = "0"
            a = strtonum("0x" addr_hex)
            if (a >= s && a < e) {
                print $0
            }
        }
    '
end

function wd_highlight_stream
    while read -l line
        if string match -rq 'Page Fault|Vector 14|CR2:|Not-Present|Caused by a Write' -- "$line"
            set_color -b red white --bold
            echo "$line"
            set_color normal
        else if string match -rq 'FS_WRITE|rax=2.*rdi=1|history' -- "$line"
            set_color yellow --bold
            echo "$line"
            set_color normal
        else if string match -rq '\[ELF\]|\[SYSCALL\]|exiting|unloaded' -- "$line"
            set_color cyan
            echo "$line"
            set_color normal
        else
            echo "$line"
        end
    end
end

function wd_log_filter
    if test (count $argv) -lt 1
        echo "Usage: wd_log_filter <logfile>" >&2
        return 1
    end
    set -l logf $argv[1]
    if not test -f "$logf"
        echo "Error: log file '$logf' not found" >&2
        return 1
    end
    grep -E 'Page Fault|Vector 14|CR2:|RSP:|RFLAGS:|CR3|FS_WRITE|rax=2|\[ELF\]|\[SYSCALL\]|exiting|unloaded|history' "$logf" \
        | wd_highlight_stream
end

function wd_extract_faults
    if test (count $argv) -lt 1
        echo "Usage: wd_extract_faults <logfile>" >&2
        return 1
    end
    set -l logf $argv[1]
    if not test -f "$logf"
        echo "Error: log file '$logf' not found" >&2
        return 1
    end
    awk '
        /Page Fault|Vector 14/ { capturing = 1; block = ""; ctx = prev1 "\n" prev2 "\n" }
        capturing {
            block = block $0 "\n"
            if ($0 ~ /^RFLAGS:/ || $0 ~ /^idiot@/) {
                print "----------------------------------------"
                printf "%s", ctx
                printf "%s", block
                capturing = 0
            }
        }
        { prev2 = prev1; prev1 = $0 }
    ' "$logf"
end

function wd_qemu_watch
    if not command -q qemu-system-x86_64
        echo "Error: qemu-system-x86_64 not in PATH" >&2
        return 1
    end
    set -l ts (date +%Y%m%d_%H%M%S)
    set -l raw_log "/tmp/windoge_qemu_$ts.log"
    echo "Logging raw output to $raw_log (Ctrl+A X to exit QEMU)"
    qemu-system-x86_64 $argv 2>&1 | tee "$raw_log" | wd_highlight_stream
end

function wd_quick_check
    set -l prog "calc"
    if test (count $argv) -ge 1
        set prog $argv[1]
    end

    if test -e "$WD_SHELL_BIN"
        wd_bss_symbols "$WD_SHELL_BIN" 10
        echo ""
        wd_addr_lookup "$WD_SHELL_BIN" "0x421000"
        echo ""
    end

    set -l prog_path (wd_resolve_bin "$prog" 2>/dev/null)
    if test -n "$prog_path"
        wd_elf_info "$prog_path"
        echo ""
        wd_bss_symbols "$prog_path" 10
    end
end
if test (count $argv) -lt 1
    wd_help
    exit 1
end

set -l cmd $argv[1]
set -l rest $argv[2..-1]

switch "$cmd"
    case bss
        wd_bss_symbols $rest
    case addr
        wd_addr_lookup $rest
    case elf
        wd_elf_info $rest
    case range
        wd_range_symbols $rest
    case log
        wd_log_filter $rest
    case faults
        wd_extract_faults $rest
    case qemu
        wd_qemu_watch $rest
    case all
        wd_quick_check $rest
    case help -h --help
        wd_help
    case '*'
        echo "Unknown command: $cmd" >&2
        wd_help
        exit 1
end