#!/bin/bash

ELF="kernel.elf"
ADDR="${1:-0xffffffff800081c1}"

echo "=== NEAREST SYMBOL ==="
nm -n "$ELF" | awk -v addr="${ADDR#0x}" '
$1 <= addr { last=$0 }
END { print last }
'

echo
echo "=== ADDR2LINE ==="
addr2line -f -C -i -e "$ELF" "$ADDR"

echo
echo "=== SOURCE + ASM AROUND ADDRESS ==="

START=$(printf "0x%X" $((ADDR - 0x100)))
STOP=$(printf "0x%X"  $((ADDR + 0x100)))

objdump -S "$ELF" \
    --start-address="$START" \
    --stop-address="$STOP"