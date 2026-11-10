import os
import sys
import shutil
import subprocess
import tempfile

architecture = os.environ.get("ARCH", "x86_64")
firmware_mode = os.environ.get("FIRMWARE", "BIOS").upper()
IMAGE_NAME = f"WindogeOS-{architecture}"

BIN_DIR = os.path.abspath("binaries")
LIMINE = os.path.join(BIN_DIR, "limine")
OVMF = os.path.join(BIN_DIR, f"ovmf-code-{architecture}.fd")

LIMINE_BIOS_SYS = os.path.join(BIN_DIR, "limine-bios.sys")
LIMINE_BIOS_CD = os.path.join(BIN_DIR, "limine-bios-cd.bin")
LIMINE_UEFI_CD = os.path.join(BIN_DIR, "limine-uefi-cd.bin")

hdd_size_mib = 32
BOOT_PARTITION_SIZE_MIB = 1

qemu_config = {
    "memory": "64M",
    "machine": [],
    "cpu": [],
    "devices": [
        "-device", "ich9-usb-uhci1",
        "-device", "usb-kbd",
        "-device", "usb-mouse"
    ],
    "firmware": []
}

boot_image_config = {
    "name": IMAGE_NAME,
    "efi_binary": None,
    "sgdisk_mbr_flags": []
}

def run_command(cmd, env=None, cwd=None):
    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            env=env,
            text=True,
            capture_output=True,
            check=True
        )
        if result.stdout.strip():
            print(result.stdout)
        if result.stderr.strip():
            print(result.stderr)
        return result
    except subprocess.CalledProcessError as e:
        print("\n================================================")
        print("BUILD FAILED")
        print("================================================")
        if e.stdout:
            print("\n--- STDOUT ---")
            print(e.stdout)
        if e.stderr:
            print("\n--- STDERR ---")
            print(e.stderr)
        print(f"\nExit code: {e.returncode}")
        sys.exit(e.returncode)

if architecture == "x86_64":
    if firmware_mode == "UEFI":
        qemu_config["machine"] = ["-M", "q35"]
    else:
        qemu_config["machine"] = ["-M", "pc"] 
        
    qemu_config["devices"] = ["-device", "qemu-xhci", "-device", "usb-kbd", "-device", "usb-mouse"]
    
    if firmware_mode == "UEFI":
        qemu_config["firmware"] = [
            "-drive",
            f"if=pflash,unit=0,format=raw,file={OVMF},readonly=on"
        ]
        boot_image_config["efi_binary"] = "BOOTX64.EFI"
    else:
        boot_image_config["efi_binary"] = "BOOTX64.EFI"
        
    boot_image_config["sgdisk_mbr_flags"] = ["-m", "1"]

else:
    if firmware_mode == "BIOS":
        sys.exit(f"Error: BIOS boot is not supported on target architecture {architecture}.")
        
    qemu_config["machine"] = ["-M", "virt"]
    qemu_config["devices"] = ["-device", "ramfb", "-device", "qemu-xhci", "-device", "usb-kbd", "-device", "usb-mouse"]
    qemu_config["firmware"] = ["-drive", f"if=pflash,unit=0,format=raw,file=binaries/ovmf-code-{architecture}.fd,readonly=on"]
    
    if architecture == "aarch64":
        qemu_config["cpu"] = ["-cpu", "cortex-a72"]
        boot_image_config["efi_binary"] = "BOOTAA64.EFI"
    elif architecture == "riscv64":
        qemu_config["cpu"] = ["-cpu", "rv64"]
        boot_image_config["efi_binary"] = "BOOTRISCV64.EFI"
    elif architecture == "loongarch64":
        qemu_config["cpu"] = ["-cpu", "la464"]
        boot_image_config["efi_binary"] = "BOOTLOONGARCH64.EFI"

def verify_build_dependencies():
    required = [LIMINE, LIMINE_UEFI_CD]

    if architecture == "x86_64":
        required += [LIMINE_BIOS_SYS, LIMINE_BIOS_CD]

    if firmware_mode == "UEFI":
        required.append(OVMF)

    missing = [f for f in required if not os.path.isfile(f)]

    if missing:
        print("\nMissing binaries:\n")
        for f in missing:
            print(f"  {f}")
        sys.exit(1)

def find_clang():
    for name in ["clang", "clang-18", "clang-17", "clang-16", "clang-15", "clang-14", "clang-13", "clang-12"]:
        path = shutil.which(name)
        if path:
            return path
    return "clang"

def compile_kernel_binary():
    verify_build_dependencies()
    env = os.environ.copy()

    clang_path = find_clang()
    clangxx_path = shutil.which("clang++") or "clang++"
    lld_path = shutil.which("ld.lld") or "ld.lld"
    nasm_path = shutil.which("nasm") or "nasm"

    if architecture == "x86_64":
        linker_script = os.path.abspath("linkers/x86_64.ld")
    elif architecture == "aarch64":
        linker_script = os.path.abspath("linkers/arm64.ld")
    else:
        linker_script = os.path.abspath(f"linkers/{architecture}.ld")

    if not os.path.isfile(linker_script):
        print(f"Error: Linker script not found at {linker_script}")
        sys.exit(1)

    obj_dir = os.path.abspath(f"kernel/bin-{architecture}")
    os.makedirs(obj_dir, exist_ok=True)
    kernel_output = os.path.abspath("kernel.elf")

    kernel_src_dir = os.path.abspath("kernel")
    source_files = []
    if os.path.exists(kernel_src_dir):
        for root, dirs, files in os.walk(kernel_src_dir):
            if "bin-" in root:
                continue
            for file in files:
                if file.endswith((".c", ".cpp", ".cc", ".s", ".S", ".asm")):
                    source_files.append(os.path.join(root, file))

    if not source_files:
        print("Error: No kernel source files found in 'kernel/' directory!")
        sys.exit(1)

    target_triples = {
        "x86_64": "x86_64-elf",
        "aarch64": "aarch64-elf",
        "riscv64": "riscv64-elf",
        "loongarch64": "loongarch64-elf"
    }
    target_triple = target_triples.get(architecture, f"{architecture}-elf")

    object_files = []
    print(f"[BUILD] Compiling kernel for target triple '{target_triple}'...")

    for src in source_files:
        rel_path = os.path.relpath(src, kernel_src_dir)
        obj_name = os.path.splitext(rel_path)[0] + ".o"
        obj_path = os.path.join(obj_dir, obj_name)
        os.makedirs(os.path.dirname(obj_path), exist_ok=True)

        if src.endswith(".asm") and architecture == "x86_64":
            compile_cmd = [
                nasm_path,
                "-felf64",
                src,
                "-o", obj_path
            ]
        elif src.endswith((".asm", ".s", ".S")) and architecture != "x86_64":
            compile_cmd = [
                clang_path,
                f"--target={target_triple}",
                "-c", src,
                "-o", obj_path
            ]
        elif src.endswith((".cpp", ".cc")):
            compile_cmd = [
                clangxx_path,
                f"--target={target_triple}",
                "-Ikernel/headers",
                "-ffreestanding",
                "-fno-pie",
                "-mcmodel=kernel",
                "-mno-sse",
                "-mno-mmx",
                "-mno-sse2",
                "-mno-avx",
                "-fno-stack-protector",
                "-c", src,
                "-o", obj_path
            ]
            if architecture == "x86_64":
                compile_cmd.extend(["-mno-red-zone", "-m64"])
        else:
            compile_cmd = [
                clang_path,
                f"--target={target_triple}",
                "-Ikernel/headers",
                "-ffreestanding",
                "-fno-pie",
                "-mcmodel=kernel",
                "-fno-stack-protector",
                "-mno-sse",
                "-mno-mmx",
                "-mno-sse2",
                "-mno-avx",
                "-c", src,
                "-o", obj_path
            ]
            if architecture == "x86_64":
                compile_cmd.extend(["-mno-red-zone", "-m64"])

        run_command(compile_cmd, env=env)
        object_files.append(obj_path)

    print(f"[LINK] Linking kernel binary to {kernel_output} using {lld_path}...")
    link_cmd = [
        lld_path,
        "-nostdlib",
        "-static",
        "-T", linker_script,
        "-o", kernel_output
    ] + object_files

    run_command(link_cmd, env=env)
    print(f"[SUCCESS] Kernel compiled successfully at {kernel_output}")
    return obj_dir

def compile_userland_programs():
    programs_dir = os.path.abspath("programs")
    if not os.path.exists(programs_dir):
        print("[Build Info] 'programs/' directory not found. Skipping userland compilation.")
        return []

    linker_script = os.path.abspath("linkers/apps_elf.ld")
    if not os.path.isfile(linker_script):
        print(f"Error: Userland linker script not found at {linker_script}")
        sys.exit(1)

    clang_path = find_clang()
    clangxx_path = shutil.which("clang++") or "clang++"

    target_triples = {
        "x86_64": "x86_64-elf",
        "aarch64": "aarch64-elf",
        "riscv64": "riscv64-elf",
        "loongarch64": "loongarch64-elf"
    }
    target_triple = target_triples.get(architecture, f"{architecture}-elf")

    binaries = []
    out_dir = os.path.abspath(f"programs/bin-{architecture}")
    os.makedirs(out_dir, exist_ok=True)

    print(f"[BUILD] Compiling userland programs in 'programs/' using linker {linker_script}...")

    for item in os.listdir(programs_dir):
        item_path = os.path.join(programs_dir, item)
        if item.startswith("bin-"):
            continue

        if os.path.isfile(item_path) and item.endswith((".c", ".cpp", ".cc", ".s", ".S")):
            prog_name = os.path.splitext(item)[0]
            obj_path = os.path.join(out_dir, f"{prog_name}.o")
            elf_path = os.path.join(out_dir, prog_name)

            is_cpp = item.endswith((".cpp", ".cc"))
            compiler_driver = clangxx_path if is_cpp else clang_path

            if is_cpp:
                compile_cmd = [
                    clangxx_path, f"--target={target_triple}",
                    "-ffreestanding", "-fno-pie", "-fno-stack-protector",
                    "-c", item_path, "-o", obj_path
                ]
            else:
                compile_cmd = [
                    clang_path, f"--target={target_triple}",
                    "-ffreestanding", "-fno-pie", "-fno-stack-protector",
                    "-c", item_path, "-o", obj_path
                ]
            if architecture == "x86_64":
                compile_cmd.extend(["-mno-red-zone", "-m64"])

            run_command(compile_cmd)

            link_cmd = [
                compiler_driver,
                f"--target={target_triple}",
                "-nostdlib",
                "-static",
                "-T", linker_script,
                "-o", elf_path,
                obj_path
            ]
            if architecture == "x86_64":
                link_cmd.append("-m64")

            run_command(link_cmd)
            binaries.append(elf_path)
            print(f"[SUCCESS] Compiled userland program: {prog_name}")

        elif os.path.isdir(item_path):
            prog_name = item
            sub_sources = []
            for root, dirs, files in os.walk(item_path):
                for f in files:
                    if f.endswith((".c", ".cpp", ".cc", ".s", ".S")):
                        sub_sources.append(os.path.join(root, f))

            if not sub_sources:
                continue

            obj_files = []
            has_cpp_files = False
            for src in sub_sources:
                rel = os.path.relpath(src, item_path)
                obj_p = os.path.join(out_dir, prog_name, rel.replace(os.path.sep, "_") + ".o")
                os.makedirs(os.path.dirname(obj_p), exist_ok=True)

                if src.endswith((".cpp", ".cc")):
                    has_cpp_files = True
                    c_cmd = [
                        clangxx_path, f"--target={target_triple}",
                        f"-I{item_path}", "-ffreestanding", "-fno-pie", "-fno-stack-protector",
                        "-c", src, "-o", obj_p
                    ]
                else:
                    c_cmd = [
                        clang_path, f"--target={target_triple}",
                        f"-I{item_path}", "-ffreestanding", "-fno-pie", "-fno-stack-protector",
                        "-c", src, "-o", obj_p
                    ]
                if architecture == "x86_64":
                    c_cmd.extend(["-mno-red-zone", "-m64"])
                run_command(c_cmd)
                obj_files.append(obj_p)

            elf_path = os.path.join(out_dir, prog_name)
            package_driver = clangxx_path if has_cpp_files else clang_path

            link_cmd = [
                package_driver,
                f"--target={target_triple}",
                "-nostdlib",
                "-static",
                "-T", linker_script,
                "-o", elf_path
            ] + obj_files
            if architecture == "x86_64":
                link_cmd.append("-m64")

            run_command(link_cmd)
            binaries.append(elf_path)
            print(f"[SUCCESS] Compiled userland program package: {prog_name}")

    return binaries

def cleanup_kernel_elf():
    if os.path.exists("kernel.elf"):
        os.remove("kernel.elf")

def generate_hdd_image():
    obj_dir = compile_kernel_binary()
    userland_binaries = compile_userland_programs()
    
    img_target = f"{boot_image_config['name']}.img"
    print(f"Compiling structured dual-partition disk ({firmware_mode} mode): {img_target}")
    
    if os.path.exists(img_target): 
        os.remove(img_target)
    
    subprocess.run(["dd", "if=/dev/zero", f"bs=1M", f"count={hdd_size_mib}", f"of={img_target}"], stdout=subprocess.DEVNULL, check=True)
        
    system_environment = os.environ.copy()
    system_environment["PATH"] = system_environment["PATH"] + ":/usr/sbin:/sbin"

    partition_type_p1 = "ef00" if firmware_mode == "UEFI" else "8300"
    p2_start = 4096
    total_sectors = (hdd_size_mib * 1024 * 1024) // 512
    p2_end = total_sectors - 2048

    sgdisk_command = [
        "sgdisk",
        img_target,
        "-n", "1:2048:4095",
        "-t", f"1:{partition_type_p1}",
        "-n", f"2:{p2_start}:{p2_end}",
        "-t", "2:8300"
    ]
    if firmware_mode == "BIOS":
        sgdisk_command.append("-m")
        sgdisk_command.append("1")

    subprocess.run(sgdisk_command, env=system_environment, check=True)
    
    if firmware_mode == "BIOS" and architecture == "x86_64" and os.path.exists(LIMINE):
        subprocess.run([LIMINE, "bios-install", img_target], stdout=subprocess.DEVNULL, check=True)
        
    boot_offset = 1048576
    esp_image = None
    if firmware_mode == "UEFI":
        esp_fd, esp_image = tempfile.mkstemp(prefix="windoge-esp-", suffix=".img")
        os.close(esp_fd)
        with open(esp_image, "wb") as esp_file:
            esp_file.truncate(boot_offset)
        boot_volume = esp_image
    else:
        boot_volume = f"{img_target}@@{boot_offset}"

    try:
        subprocess.run(["mformat", "-i", boot_volume, "-v", "BOOT"], check=True)
        subprocess.run(["mmd", "-i", boot_volume, "::/boot"], check=True)
        subprocess.run(["mmd", "-i", boot_volume, "::/boot/limine"], check=True)
        
        if firmware_mode == "UEFI":
            subprocess.run(["mmd", "-i", boot_volume, "::/EFI"], check=True)
            subprocess.run(["mmd", "-i", boot_volume, "::/EFI/BOOT"], check=True)

        subprocess.run(["mcopy", "-i", boot_volume, "kernel.elf", "::/boot/kernel.elf"], check=True)

        if os.path.exists("limine.conf"):
            subprocess.run(["mcopy", "-i", boot_volume, "limine.conf", "::/limine.conf"], check=True)
            subprocess.run(["mcopy", "-i", boot_volume, "limine.conf", "::/boot/limine/limine.conf"], check=True)

        if firmware_mode == "BIOS" and architecture == "x86_64":
            if os.path.exists(LIMINE_BIOS_SYS):
                subprocess.run(["mcopy", "-i", boot_volume, LIMINE_BIOS_SYS, "::/boot/limine/limine-bios.sys"], check=True)
        else:
            uefi_bootloader = os.path.join("binaries", boot_image_config["efi_binary"]) if boot_image_config.get("efi_binary") else ""
            if uefi_bootloader and os.path.exists(uefi_bootloader):
                efi_filename = boot_image_config["efi_binary"]
                subprocess.run(["mcopy", "-i", boot_volume, uefi_bootloader, f"::/EFI/BOOT/{efi_filename}"], check=True)
            
        if firmware_mode == "UEFI":
            subprocess.run(["dd", f"if={esp_image}", f"of={img_target}", "bs=512", "seek=2048", "conv=notrunc"], stdout=subprocess.DEVNULL, check=True)
    finally:
        if esp_image is not None and os.path.exists(esp_image):
            os.remove(esp_image)

    part2_img = "part2_ext2.img"
    p2_sectors = p2_end - p2_start + 1
    subprocess.run(["dd", "if=/dev/zero", f"of={part2_img}", "bs=512", f"count={p2_sectors}"], stdout=subprocess.DEVNULL, check=True)
    
    subprocess.run(["mkfs.ext2", "-F", "-b", "1024", "-O", "^dir_index,^filetype", "-L", "WINDOGEOS", part2_img], check=True)

    with open(part2_img, "r+b") as f:
        f.seek(1024 + 56)
        magic = f.read(2)
        print(f"[Build Check] Read Ext2 magic from part2_ext2.img: {magic.hex()}")
        if magic != b'\x53\xef':
            print("[Build Warning] Magic number missing! Force-injecting 0xEF53...")
            f.seek(1024 + 56)
            f.write(bytes([0x53, 0xef]))
            f.seek(1024 + 0)
            f.write((p2_sectors // 2).to_bytes(4, 'little'))
            f.seek(1024 + 4)
            f.write((p2_sectors // 2).to_bytes(4, 'little'))
            f.seek(1024 + 24)
            f.write((0).to_bytes(4, 'little'))

    mnt_dir = "/tmp/windoge_p2_mnt"
    os.makedirs(mnt_dir, exist_ok=True)

    injected_programs = []
    
    loop_dev = subprocess.check_output(f"sudo losetup -f --show {part2_img}", shell=True).decode().strip()
    subprocess.run(f"sudo mount {loop_dev} {mnt_dir}", shell=True, check=True)

    try:
        for bin_path in userland_binaries:
            if os.path.exists(bin_path):
                prog_basename = os.path.basename(bin_path)
                subprocess.run(["sudo", "cp", bin_path, f"{mnt_dir}/"], check=True)
                injected_programs.append(prog_basename)
        print("[Partition 2 Contents]:")
        subprocess.run(["sudo", "ls", "-la", mnt_dir], check=True)
        subprocess.run("sudo sync", shell=True, check=True)
    finally:
        subprocess.run(f"sudo umount {mnt_dir}", shell=True, check=True)
        subprocess.run(f"sudo losetup -d {loop_dev}", shell=True, check=True)

    if os.path.exists(mnt_dir):
        try:
            os.rmdir(mnt_dir)
        except OSError:
            pass

    subprocess.run(["dd", f"if={part2_img}", f"of={img_target}", "bs=512", f"seek={p2_start}", "conv=notrunc"], stdout=subprocess.DEVNULL, check=True)
    if os.path.exists(part2_img):
        os.remove(part2_img)

    shutil.rmtree(obj_dir, ignore_errors=True)
    shutil.rmtree(os.path.abspath(f"programs/bin-{architecture}"), ignore_errors=True)

    cmd = "objdump -d kernel.elf | grep -C 5 \"7bb1\""
    subprocess.run(cmd, shell=True)

    print("Things injected to /:")
    if injected_programs:
        for prog in injected_programs:
            print(f"PROGRAM > {prog}")
    else:
        print("  (No userland programs found/injected)")

    print(f"[SUCCESS] Disk image generated successfully: {img_target}")

def execute_qemu_hdd():
    img_target = f"{boot_image_config['name']}.img"
    if not os.path.exists(img_target):
        print(f"Error: Disk image '{img_target}' not found. Build it first using: python compile.py all-img")
        sys.exit(1)
        
    qemu_arguments = [
        f"qemu-system-{architecture}",
        "-M", "q35",
        "-device", "ahci,id=ahci",
        "-drive", f"id=disk,file={img_target},format=raw,if=none",
        "-device", "ide-hd,drive=disk,bus=ahci.0",
        "-serial", "stdio",
        "-d", "int,cpu_reset,mmu",
        "-no-reboot",
        "-D", "qemu.log",
        "-device", "piix3-usb-uhci,id=uhci",
        "-device", "usb-kbd,bus=uhci.0"
    ]
    
    if qemu_config.get("cpu"):
        qemu_arguments += qemu_config["cpu"]
        
    config_devices = qemu_config.get("devices", [])
    i = 0
    while i < len(config_devices):
        item = config_devices[i]
        if item == "-device" and i + 1 < len(config_devices):
            dev_val = config_devices[i+1]
            if any(x in dev_val for x in ["xhci", "uhci", "usb-kbd", "usb-mouse"]):
                i += 2
                continue
        qemu_arguments.append(item)
        i += 1
        
    qemu_arguments += ["-m", qemu_config["memory"]]
    
    if qemu_config.get("firmware"):
        qemu_arguments += qemu_config["firmware"]
        
    print(f"[QEMU] Running command: {' '.join(qemu_arguments)}")
    
    with open("windoge.log", "w") as log_file:
        process = subprocess.Popen(
            qemu_arguments,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0
        )
        for line in process.stdout:
            decoded_line = line.decode('utf-8', errors='replace')
            sys.stdout.write(decoded_line)
            sys.stdout.flush()
            log_file.write(decoded_line)
            log_file.flush()
        process.wait()

if __name__ == "__main__":
    requested_action = (
        sys.argv[1]
        if len(sys.argv) > 1
        else "all-img"
    )
    if requested_action in ["all-img", "all"]:
        generate_hdd_image()
    elif requested_action in ["run", "run-img"]:
        execute_qemu_hdd()