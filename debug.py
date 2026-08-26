import sys
import re

EXCEPTION_NAMES = {
    0: "Divide Error (#DE)",
    1: "Debug (#DB)",
    2: "NMI Interrupt",
    3: "Breakpoint (#BP)",
    4: "Overflow (#OF)",
    5: "BOUND Range Exceeded (#BR)",
    6: "Invalid Opcode (#UD)",
    7: "Device Not Available (#NM)",
    8: "Double Fault (#DF)",
    9: "Coprocessor Segment Overrun",
    10: "Invalid TSS (#TS)",
    11: "Segment Not Present (#NP)",
    12: "Stack-Segment Fault (#SS)",
    13: "General Protection Fault (#GP)",
    14: "Page Fault (#PF)",
    16: "x87 FPU Floating-Point Error (#MF)",
    17: "Alignment Check (#AC)",
    18: "Machine Check (#MC)",
    19: "SIMD Floating-Point Exception (#XM)",
    20: "Virtualization Exception (#VE)",
    21: "Control Protection Exception (#CP)",
}

def analyze_exceptions(filename="qemu.log"):
    try:
        with open(filename, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Error: '{filename}' not found.")
        return

    lines = content.splitlines()
    print(f"Loaded {len(lines)} lines from {filename}. Parsing exceptions...\n")

    ex_regex = re.compile(r"v=([0-9a-fA-F]+)|check_exception.*new\s+(0x[0-9a-fA-F]+|[0-9]+)", re.IGNORECASE)

    found_exceptions = []

    for idx, line in enumerate(lines):
        match = ex_regex.search(line)
        if match:
            vector_str = match.group(1) or match.group(2)
            if vector_str:
                vector_val = int(vector_str, 16)
                name = EXCEPTION_NAMES.get(vector_val, f"Unknown Exception (Vector {vector_val})")
                found_exceptions.append((idx + 1, vector_val, name, line.strip()))

    print(f"Total exceptions found: {len(found_exceptions)}\n")

    for line_num, vec, name, text in found_exceptions:
        print(f"Line {line_num} | Vector {vec:02d} ({name})")
        print(f"  -> {text}\n")

    if not found_exceptions:
        print("No explicit CPU exceptions (v=XX) found in the log.")

if __name__ == "__main__":
    target_log = sys.argv[1] if len(sys.argv) > 1 else "qemu.log"
    analyze_exceptions(target_log)