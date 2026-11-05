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

def decode_pf_error(err_code):
    p = err_code & 1
    wr = (err_code >> 1) & 1
    us = (err_code >> 2) & 1
    id_ = (err_code >> 4) & 1
    
    if id_ == 1:
        return "attempted to fetch instruction from unmapped memory at"
    elif wr == 1:
        if p == 1:
            return "attempted to write to a read-only memory page at"
        return "attempted to write to unmapped memory at"
    else:
        if p == 1:
            return "attempted to read a restricted memory page at"
        return "attempted to read unmapped memory at"

def analyze_exceptions(filename="qemu.log"):
    try:
        with open(filename, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Error: '{filename}' not found.")
        return

    lines = content.splitlines()
    print(f"Loaded {len(lines)} lines from {filename}. Parsing exceptions and scheduler transitions...\n")

    check_ex_regex = re.compile(r"check_exception\s+old:\s*(0x[0-9a-fA-F]+|[0-9a-fA-F\-]+)\s+new\s+(0x[0-9a-fA-F]+|[0-9a-fA-F]+)", re.IGNORECASE)
    dump_regex = re.compile(r"v=([0-9a-fA-F]+)(?:\s+e=([0-9a-fA-F]+))?(?:\s+i=(\d+))?(?:\s+cpl=(\d+))?(?:\s+IP=([0-9a-fA-F\:]+))?(?:\s+pc=([0-9a-fA-F]+))?(?:\s+SP=([0-9a-fA-F\:]+))?", re.IGNORECASE)
    cr2_regex = re.compile(r"CR2=([0-9a-fA-F]+)", re.IGNORECASE)
    cr3_regex = re.compile(r"CR3=([0-9a-fA-F]+)", re.IGNORECASE)
    
    # Custom traps for common kernel print patterns during scheduling
    switch_regex = re.compile(r"(?:switch|schedule|task|proc).*(?:to|from|prev|next).*([0-9a-fA-F]+)", re.IGNORECASE)

    for idx, line in enumerate(lines):
        clean_line = line.strip()
        vector_val = None
        sentence = ""

        match_check = check_ex_regex.search(clean_line)
        match_dump = dump_regex.search(clean_line)
        match_switch = switch_regex.search(clean_line)

        if match_check:
            new_str = match_check.group(2)
            vector_val = int(new_str, 16) if '0x' in new_str else int(new_str, 16)
            old_str = match_check.group(1)
            
            if '0x' in old_str: old_val = int(old_str, 16) & 0xffffffff
            elif '-' in old_str: old_val = int(old_str) & 0xffffffff
            else: old_val = int(old_str, 16) & 0xffffffff
                
            old_name = "None" if old_val == 0xffffffff else EXCEPTION_NAMES.get(old_val, f"Vector {old_val}")
            new_name = EXCEPTION_NAMES.get(vector_val, f"Vector {vector_val}")
            sentence = f"cpu internal cascade check: escalating from {old_name} handler to {new_name}."

        elif match_dump:
            v_str = match_dump.group(1)
            vector_val = int(v_str, 16)
            if vector_val >= 32:
                continue
                
            e_str = match_dump.group(2)
            e_val = int(e_str, 16) if e_str else 0
            
            cr2_match = cr2_regex.search(clean_line)
            cr2_val = cr2_match.group(1) if cr2_match else "unknown"
            
            cr3_match = cr3_regex.search(clean_line)
            cr3_suffix = f" with active CR3 directory page root at 0x{cr3_match.group(1)}" if cr3_match else ""

            if vector_val == 0:
                sentence = "attempted division by zero or division result overflowed destination register."
            elif vector_val == 1:
                sentence = "hit hardware debug trap or active register single-step condition."
            elif vector_val == 6:
                sentence = "attempted to execute an invalid, undefined, or malformed CPU opcode."
            elif vector_val == 12:
                sentence = f"stack operation caused a segment limit violation or dynamic overflow with segment descriptor selector 0x{e_val:X}."
            elif vector_val == 13:
                sentence = f"general protection fault occurred violating system privilege rules or memory limits with index descriptor selector 0x{e_val:X}."
            elif vector_val == 14:
                action = decode_pf_error(e_val)
                sentence = f"{action} 0x{cr2_val}{cr3_suffix} (error code: 0x{e_val:04X})."
            else:
                sentence = f"triggered hardware vector fault execution index code 0x{e_val:X}."

        elif match_switch:
            # Traps context switch debug strings if your OS prints them
            sentence = f"kernel logging thread context transition state or descriptor pointer alteration near reference 0x{match_switch.group(1)}."
            print(f"Line {idx + 1} | Scheduler Tracking Event")
            print(f"  -> {clean_line}")
            print(f"  -> {sentence}\n")
            continue

        if vector_val is not None and vector_val < 32:
            name = EXCEPTION_NAMES.get(vector_val, f"Unknown Exception (Vector {vector_val})")
            print(f"Line {idx + 1} | Vector {vector_val} ({name})")
            print(f"  -> {clean_line}")
            print(f"  -> {sentence}")
            print()

if __name__ == "__main__":
    target_log = sys.argv if len(sys.argv) > 1 else "qemu.log"
    analyze_exceptions(target_log)
