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

hdd_size_mib = 6
BOOT_PARTITION_SIZE_MIB = 1

qemu_config = {
    "memory": "5M",
    "machine": [],
    "cpu": [],
    "devices": [
        "-device", "qemu-xhci,id=xhci",
        "-device", "usb-kbd,bus=xhci.0"
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
        qemu_config["firmware"] = [
            "-drive",
            f"if=pflash,unit=0,format=raw,file={OVMF},readonly=on"
        ]
    elif firmware_mode == "BIOS":
        qemu_config["machine"] = ["-M", "pc"]
        boot_image_config["sgdisk_mbr_flags"] = ["-m", "1"]
    else:
        sys.exit(f"Error: Unsupported firmware mode '{firmware_mode}'.")

    qemu_config["devices"] = ["-device", "qemu-xhci,id=xhci", "-device", "usb-kbd,bus=xhci.0"]
    boot_image_config["efi_binary"] = "BOOTX64.EFI"
else:
    if firmware_mode == "BIOS":
        sys.exit(f"Error: BIOS boot is not supported on target architecture {architecture}.")
    if firmware_mode != "UEFI":
        sys.exit(f"Error: Unsupported firmware mode '{firmware_mode}'.")

    qemu_config["machine"] = ["-M", "virt"]
    qemu_config["devices"] = ["-device", "ramfb", "-device", "qemu-xhci,id=xhci", "-device", "usb-kbd,bus=xhci.0"]
    qemu_config["firmware"] = ["-drive", f"if=pflash,unit=0,format=raw,file={OVMF},readonly=on"]

    if architecture == "aarch64":
        qemu_config["cpu"] = ["-cpu", "cortex-a72"]
        boot_image_config["efi_binary"] = "BOOTAA64.EFI"
    elif architecture == "riscv64":
        qemu_config["cpu"] = ["-cpu", "rv64"]
        boot_image_config["efi_binary"] = "BOOTRISCV64.EFI"
    elif architecture == "loongarch64":
        qemu_config["cpu"] = ["-cpu", "la464"]
        boot_image_config["efi_binary"] = "BOOTLOONGARCH64.EFI"
    else:
        sys.exit(f"Error: Unsupported architecture '{architecture}'.")

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
        return [], []

    linker_script = os.path.abspath("linkers/apps_elf.ld")
    if not os.path.isfile(linker_script):
        print(f"Error: Userland linker script not found at {linker_script}")
        sys.exit(1)

    clang_path = find_clang()
    clangxx_path = shutil.which("clang++") or "clang++"
    nasm_path = shutil.which("nasm") or "nasm"
    lld_path = shutil.which("ld.lld") or "ld.lld"

    target_triples = {
        "x86_64": "x86_64-elf",
        "aarch64": "aarch64-elf",
        "riscv64": "riscv64-elf",
        "loongarch64": "loongarch64-elf"
    }
    target_triple = target_triples.get(architecture, f"{architecture}-elf")

    lld_emulations = {
        "x86_64": "elf_x86_64",
        "aarch64": "aarch64elf",
        "riscv64": "elf64lriscv",
        "loongarch64": "elf64loongarch"
    }
    lld_emulation = lld_emulations.get(architecture)

    def get_c_flags(compiler, include_dir=None):
        cmd = [
            compiler,
            f"--target={target_triple}",
            "-ffreestanding",
            "-fno-pie",
            "-fno-stack-protector",
            "-fno-builtin"
        ]
        if include_dir:
            cmd.append(f"-I{include_dir}")
        if architecture == "x86_64":
            cmd.extend([
                "-mno-red-zone",
                "-m64",
                "-mno-sse",
                "-mno-sse2",
                "-mno-mmx",
                "-mno-avx"
            ])
        return cmd

    def make_lld_cmd(elf_path, obj_list):
        cmd = [
            lld_path,
            "-nostdlib",
            "-static",
            "-z", "separate-code",
            "-T", linker_script,
            "-o", elf_path
        ]
        if lld_emulation:
            cmd.extend(["-m", lld_emulation])
        cmd.extend(obj_list)
        return cmd

    def stamp_windoge_abi(elf_path):
        with open(elf_path, "r+b") as f:
            if f.read(4) != b"\x7fELF":
                raise RuntimeError(f"Output file is not a valid ELF binary: {elf_path}")
            f.seek(7)
            f.write(b"\x44")

    user_binaries = []
    system_binaries = []

    out_dir = os.path.abspath(f"programs/bin-{architecture}")
    sys_out_dir = os.path.join(out_dir, "system")
    os.makedirs(out_dir, exist_ok=True)
    os.makedirs(sys_out_dir, exist_ok=True)

    print(f"[BUILD] Compiling userland and system programs using {lld_path} and {linker_script}...")

    def compile_items_in(source_base, destination_dir, target_list, is_system_folder=False):
        if not os.path.exists(source_base):
            return
        for item in os.listdir(source_base):
            item_path = os.path.join(source_base, item)
            if item.startswith("bin-") or (not is_system_folder and item == "system"):
                continue

            if os.path.isfile(item_path) and item.endswith((".c", ".cpp", ".cc", ".s", ".S", ".asm")):
                prog_name, _ = os.path.splitext(item)
                obj_path = os.path.join(destination_dir, f"{prog_name}.o")
                elf_path = os.path.join(destination_dir, prog_name)

                is_cpp = item.endswith((".cpp", ".cc"))
                is_asm = item.endswith(".asm")

                if is_asm:
                    if architecture != "x86_64":
                        print("Error: .asm programs require x86_64 architecture for NASM.")
                        sys.exit(1)
                    run_command([nasm_path, "-felf64", item_path, "-o", obj_path])
                else:
                    compiler = clangxx_path if is_cpp else clang_path
                    compile_cmd = get_c_flags(compiler) + ["-c", item_path, "-o", obj_path]
                    run_command(compile_cmd)

                link_cmd = make_lld_cmd(elf_path, [obj_path])
                run_command(link_cmd)
                stamp_windoge_abi(elf_path)
                target_list.append(elf_path)
                print(f"[SUCCESS] Compiled program: {prog_name}")

            elif os.path.isdir(item_path):
                prog_name = item
                sub_sources = []
                for root, dirs, files in os.walk(item_path):
                    for f in files:
                        if f.endswith((".c", ".cpp", ".cc", ".s", ".S", ".asm")):
                            sub_sources.append(os.path.join(root, f))

                if not sub_sources:
                    continue

                obj_files = []
                sub_out_dir = os.path.join(destination_dir, prog_name)
                os.makedirs(sub_out_dir, exist_ok=True)

                for src in sub_sources:
                    rel = os.path.relpath(src, item_path)
                    obj_p = os.path.join(sub_out_dir, rel.replace(os.path.sep, "_") + ".o")
                    os.makedirs(os.path.dirname(obj_p), exist_ok=True)

                    if src.endswith(".asm"):
                        if architecture != "x86_64":
                            print("Error: .asm packages require x86_64 architecture for NASM.")
                            sys.exit(1)
                        c_cmd = [nasm_path, "-felf64", src, "-o", obj_p]
                    elif src.endswith((".cpp", ".cc")):
                        c_cmd = get_c_flags(clangxx_path, item_path) + ["-c", src, "-o", obj_p]
                    else:
                        c_cmd = get_c_flags(clang_path, item_path) + ["-c", src, "-o", obj_p]

                    run_command(c_cmd)
                    obj_files.append(obj_p)

                elf_path = os.path.join(destination_dir, prog_name)
                link_cmd = make_lld_cmd(elf_path, obj_files)

                run_command(link_cmd)
                stamp_windoge_abi(elf_path)
                target_list.append(elf_path)
                print(f"[SUCCESS] Compiled package: {prog_name}")

    compile_items_in(programs_dir, out_dir, user_binaries, is_system_folder=False)

    sys_dir = os.path.join(programs_dir, "system")
    compile_items_in(sys_dir, sys_out_dir, system_binaries, is_system_folder=True)

    return user_binaries, system_binaries

def cleanup_kernel_elf():
    if os.path.exists("kernel.elf"):
        os.remove("kernel.elf")

def generate_hdd_image():
    obj_dir = compile_kernel_binary()
    user_binaries, system_binaries = compile_userland_programs()

    img_target = f"{boot_image_config['name']}.img"
    print(f"Compiling structured dual-partition disk ({firmware_mode} mode): {img_target}")

    if os.path.exists(img_target):
        os.remove(img_target)

    with open(img_target, "wb") as f:
        f.truncate(hdd_size_mib * 1024 * 1024)

    system_environment = os.environ.copy()
    system_environment["PATH"] = system_environment.get("PATH", "") + ":/usr/sbin:/sbin"

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
    sgdisk_command.extend(boot_image_config["sgdisk_mbr_flags"])

    run_command(sgdisk_command, env=system_environment)

    if firmware_mode == "BIOS" and architecture == "x86_64" and os.path.exists(LIMINE):
        run_command([LIMINE, "bios-install", img_target], env=system_environment)

    boot_offset = 2048 * 512
    boot_size = (4095 - 2048 + 1) * 512
    esp_image = None
    if firmware_mode == "UEFI":
        esp_fd, esp_image = tempfile.mkstemp(prefix="windoge-esp-", suffix=".img")
        os.close(esp_fd)
        with open(esp_image, "wb") as esp_file:
            esp_file.truncate(boot_size)
        boot_volume = esp_image
    else:
        boot_volume = f"{img_target}@@{boot_offset}"

    try:
        run_command(["mformat", "-i", boot_volume, "-v", "BOOT"], env=system_environment)
        run_command(["mmd", "-i", boot_volume, "::/boot"], env=system_environment)
        run_command(["mmd", "-i", boot_volume, "::/boot/limine"], env=system_environment)

        if firmware_mode == "UEFI":
            run_command(["mmd", "-i", boot_volume, "::/EFI"], env=system_environment)
            run_command(["mmd", "-i", boot_volume, "::/EFI/BOOT"], env=system_environment)

        run_command(["mcopy", "-i", boot_volume, "kernel.elf", "::/boot/kernel.elf"], env=system_environment)

        if os.path.exists("limine.conf"):
            run_command(["mcopy", "-i", boot_volume, "limine.conf", "::/limine.conf"], env=system_environment)
            run_command(["mcopy", "-i", boot_volume, "limine.conf", "::/boot/limine/limine.conf"], env=system_environment)

        if firmware_mode == "BIOS" and architecture == "x86_64":
            if os.path.exists(LIMINE_BIOS_SYS):
                run_command(["mcopy", "-i", boot_volume, LIMINE_BIOS_SYS, "::/boot/limine/limine-bios.sys"], env=system_environment)
        else:
            efi_filename = boot_image_config.get("efi_binary")
            uefi_bootloader = os.path.join(BIN_DIR, efi_filename) if efi_filename else ""
            if uefi_bootloader and os.path.exists(uefi_bootloader):
                run_command(["mcopy", "-i", boot_volume, uefi_bootloader, f"::/EFI/BOOT/{efi_filename}"], env=system_environment)

        if firmware_mode == "UEFI" and esp_image:
            with open(esp_image, "rb") as src_f, open(img_target, "r+b") as dst_f:
                dst_f.seek(boot_offset)
                shutil.copyfileobj(src_f, dst_f)
    finally:
        if esp_image is not None and os.path.exists(esp_image):
            os.remove(esp_image)

    p2_offset_bytes = p2_start * 512
    p2_bytes_count = (p2_end - p2_start + 1) * 512

    mnt_dir = tempfile.mkdtemp(prefix="windoge_p2_mnt_")

    injected_programs = []
    injected_systems = []

    loop_dev = subprocess.check_output(
        ["sudo", "losetup", "-f", "--show", "-o", str(p2_offset_bytes), "--sizelimit", str(p2_bytes_count), img_target],
        text=True
    ).strip()

    try:
        run_command(["sudo", "mkfs.exfat", "-L", "WINDOGEOS", loop_dev], env=system_environment)
        run_command(["sudo", "mount", loop_dev, mnt_dir], env=system_environment)

        run_command(["sudo", "mkdir", "-p", f"{mnt_dir}/programs"], env=system_environment)
        run_command(["sudo", "mkdir", "-p", f"{mnt_dir}/system"], env=system_environment)
        run_command(["sudo", "mkdir", "-p", f"{mnt_dir}/users"], env=system_environment)

        for bin_path in user_binaries:
            if os.path.exists(bin_path):
                prog_basename = os.path.basename(bin_path)
                run_command(["sudo", "cp", bin_path, f"{mnt_dir}/programs/"], env=system_environment)
                injected_programs.append(prog_basename)

        for bin_path in system_binaries:
            if os.path.exists(bin_path):
                sys_basename = os.path.basename(bin_path)
                run_command(["sudo", "cp", bin_path, f"{mnt_dir}/system/"], env=system_environment)
                injected_systems.append(sys_basename)

        print("[Partition 2 /programs Contents]:")
        run_command(["sudo", "ls", "-la", f"{mnt_dir}/programs"], env=system_environment)
        print("[Partition 2 /system Contents]:")
        run_command(["sudo", "ls", "-la", f"{mnt_dir}/system"], env=system_environment)

        run_command(["sudo", "sync"], env=system_environment)
    finally:
        subprocess.run(["sudo", "umount", mnt_dir], check=False)
        subprocess.run(["sudo", "losetup", "-d", loop_dev], check=False)
        if os.path.exists(mnt_dir):
            shutil.rmtree(mnt_dir, ignore_errors=True)

    shutil.rmtree(obj_dir, ignore_errors=True)

    print("Things injected to /programs:")
    if injected_programs:
        for prog in injected_programs:
            print(f"PROGRAM > {prog}")
    else:
        print("  (No userland programs found/injected)")

    print("Things injected to /system:")
    if injected_systems:
        for sys_item in injected_systems:
            print(f"SYSTEM > {sys_item}")
    else:
        print("  (No system programs found/injected)")

    print(f"[SUCCESS] Disk image generated successfully: {img_target}")

def execute_qemu_hdd():
    img_target = f"{boot_image_config['name']}.img"
    if not os.path.exists(img_target):
        print(f"Error: Disk image '{img_target}' not found. Build it first using: python compile.py all-img")
        sys.exit(1)

    qemu_arguments = [
        f"qemu-system-{architecture}",
        "-audiodev", "pa,id=snd0",
        "-M", "q35,pcspk-audiodev=snd0",
        "-device", "ahci,id=ahci",
        "-drive", f"id=disk,file={img_target},format=raw,if=none",
        "-device", "ide-hd,drive=disk,bus=ahci.0",
        "-serial", "stdio",
        "-d", "int,cpu_reset,mmu",
        "-D", "qemu.log",
        "-device", "qemu-xhci,id=xhci",
        "-device", "usb-kbd,bus=xhci.0"
    ]

    if qemu_config.get("cpu"):
        qemu_arguments += qemu_config["cpu"]

    config_devices = qemu_config.get("devices", [])
    i = 0
    while i < len(config_devices):
        item = config_devices[i]
        if item == "-device" and i + 1 < len(config_devices):
            dev_val = config_devices[i + 1]
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
            decoded_line = line.decode("utf-8", errors="replace")
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
    else:
        sys.exit(f"Error: Unknown action '{requested_action}'.")