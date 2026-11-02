import os
import sys
import time
import argparse
import subprocess
import importlib.util
import tempfile
from concurrent.futures import ProcessPoolExecutor, as_completed

DISK_IMAGE_SIZE_MB = 12

def lz4_compress_block(data: bytes) -> bytes:
    compressed = bytearray()
    hash_table = {}

    def write_length(length: int) -> None:
        while length >= 255:
            compressed.append(255)
            length -= 255
        compressed.append(length)

    def write_sequence(anchor: int, end: int, match_length: int = 0,
                       match_offset: int = 0) -> None:
        literal_length = end - anchor
        encoded_match_length = max(0, match_length - 4)
        compressed.append(
            (min(literal_length, 15) << 4) |
            min(encoded_match_length, 15)
        )
        if literal_length >= 15:
            write_length(literal_length - 15)
        compressed.extend(data[anchor:end])

        if match_length:
            compressed.extend(match_offset.to_bytes(2, byteorder="little"))
            if encoded_match_length >= 15:
                write_length(encoded_match_length - 15)

    index = 0
    anchor = 0
    while len(data) >= 12 and index <= len(data) - 12:
        word = int.from_bytes(data[index:index + 4], "little")
        key = (word * 2654435761) >> 20
        candidate = hash_table.get(key)
        hash_table[key] = index

        if (candidate is None or index - candidate > 4096 or
                data[candidate:candidate + 4] != data[index:index + 4]):
            index += 1
            continue

        match_length = 4
        maximum_match = len(data) - 5 - index
        while (match_length < maximum_match and
               data[candidate + match_length] ==
               data[index + match_length]):
            match_length += 1

        write_sequence(anchor, index, match_length, index - candidate)
        match_end = index + match_length
        for position in range(index + 1, min(match_end, len(data) - 3)):
            word = int.from_bytes(data[position:position + 4], "little")
            hash_table[(word * 2654435761) >> 20] = position
        index = match_end
        anchor = index

    if data:
        write_sequence(anchor, len(data))
    return bytes(compressed)


def lz4_compress_data(data: bytes) -> bytes:
    if len(data) > 0xFFFFFFFF:
        raise ValueError("LZ4 package exceeds the 4 GiB format limit")

    compressed = bytearray(len(data).to_bytes(4, byteorder="little"))
    compressed.extend(b"LZ4C")
    for offset in range(0, len(data), 4096):
        block = data[offset:offset + 4096]
        encoded_block = lz4_compress_block(block)
        compressed.extend(len(block).to_bytes(4, byteorder="little"))
        compressed.extend(len(encoded_block).to_bytes(4, byteorder="little"))
        compressed.extend(encoded_block)
    return bytes(compressed)

def cmd(cmd_str, critical=True):
    res = subprocess.run(cmd_str, shell=True)
    if res.returncode != 0 and critical:
        print(f"\n[Not Wow] Build failed on command:\n  {cmd_str}")
        sys.exit(1)
    return res.returncode == 0

def run_cmd(cmd_str):
    res = subprocess.run(cmd_str, shell=True, capture_output=True, text=True)
    return res.returncode == 0, cmd_str, res.stdout, res.stderr

def print_command_output(stdout, stderr):
    if stdout:
        sys.stdout.write(stdout)
        sys.stdout.flush()
    if stderr:
        sys.stderr.write(stderr)
        sys.stderr.flush()

def run_parallel(commands):
    if not commands:
        return
    with ProcessPoolExecutor() as executor:
        futures = [executor.submit(run_cmd, c) for c in commands]
        for future in as_completed(futures):
            success, failed_cmd, stdout, stderr = future.result()
            print_command_output(stdout, stderr)
            if not success:
                print(f"\n[Not Wow] Build failed on command:\n  {failed_cmd}")
                executor.shutdown(wait=False, cancel_futures=True)
                sys.exit(1)

def cleanup():
    subprocess.run("find . -name '*.o' -type f -delete 2>/dev/null || true", shell=True)
    subprocess.run("find . -name '*.plist' -type f -delete 2>/dev/null || true", shell=True)
    if os.path.exists("part2_exfat.img"):
        try:
            os.remove("part2_exfat.img")
        except OSError:
            pass

def main():
    run_cmd("clear")
    start_time = time.perf_counter()

    try:
        app_sources = []
        package_sources = []

        compiled_app_elfs = []
        compiled_system_app_elfs = []
        compressed_packages = []

        config_path = "compile_config.py"
        if not os.path.exists(config_path):
            print(f"[Not Wow] Could not find configuration file '{config_path}'")
            sys.exit(1)

        spec = importlib.util.spec_from_file_location("compile_config", config_path)
        cfg = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cfg)

        parser = argparse.ArgumentParser(description=f"{cfg.project_name} Dual-Partition Build Engine")
        parser.add_argument("-a", "--arch", default=cfg.default_arch, help="Target architecture (default: %(default)s)")
        parser.add_argument(
            "--boot-mode",
            choices=("bios", "uefi"),
            help="Boot firmware mode (default: BIOS for x86_64, UEFI for arm64)",
        )
        parser.add_argument("--skip-analyze", action="store_true", help="Skip running Clang Static Analyzer")
        args = parser.parse_args()

        arch = args.arch
        if arch not in cfg.compiler_flags:
            print(f"[Not Wow] Unknown architecture '{arch}'. Supported options: {list(cfg.compiler_flags.keys())}")
            sys.exit(1)

        is_arm64 = arch in ["arm64", "aarch64"]
        boot_mode = args.boot_mode or ("uefi" if is_arm64 else "bios")
        if boot_mode == "bios" and is_arm64:
            print("[Not Wow] BIOS boot is only supported for x86_64.")
            sys.exit(1)

        arch_cfg = cfg.compiler_flags[arch]
        tools = cfg.tools

        common_flags = (
            "-g -Wall -Wextra -Werror -Wconversion -std=gnu11 -nostdinc -ffreestanding "
            "-fno-stack-protector -fno-stack-check -fno-lto -fno-PIC -fno-pie "
            "-ffunction-sections -fdata-sections -Iheaders -Ilibrary -Iapps"
        )

        c_source = []
        asm_source = []

        for folder in cfg.source_folders_regular:
            if os.path.exists(folder):
                for root, _, files in os.walk(folder):
                    for file in files:
                        source_path = os.path.join(root, file)
                        if file.endswith(".c"):
                            c_source.append(source_path)
                        elif file.endswith(".asm"):
                            asm_source.append(source_path)
        
        app_library_paths = {
            os.path.abspath(os.path.normpath(src))
            for src in getattr(cfg, "app_library_sources", [])
            if arch in getattr(cfg, "app_library_arches", {}).get(src, [arch])
        }
        app_source_arches = getattr(cfg, "app_source_arches", {})
        app_folders = getattr(cfg, "source_folders_apps", ["apps", "userland"])
        
        for app_dir in app_folders:
            abs_app_dir = os.path.abspath(app_dir)
            if os.path.exists(abs_app_dir):
                for root, _, files in os.walk(abs_app_dir):
                    for file in files:
                        file_path = os.path.join(root, file)
                        normalized_path = os.path.abspath(os.path.normpath(file_path))

                        if normalized_path.startswith(
                            os.path.abspath(os.path.join("apps", "text")) + os.sep
                        ):
                            continue

                        if normalized_path in app_library_paths:
                            continue

                        relative_path = os.path.normpath(os.path.relpath(file_path))

                        if arch not in app_source_arches.get(relative_path, [arch]):
                            continue

                        is_package = (
                            "apps" + os.sep + "packages" in normalized_path
                            or
                            "userland" + os.sep + "packages" in normalized_path
                        )

                        if file.endswith(".c"):
                            if is_package:
                                package_sources.append((file_path, "c"))
                            else:
                                app_sources.append((file_path, "c"))

                        elif file.endswith(".asm"):
                            if is_package:
                                package_sources.append((file_path, "asm"))
                            else:
                                app_sources.append((file_path, "asm"))

        if not c_source and not asm_source:
            print("[Not Wow] No kernel source files found.")
            sys.exit(1)

        print(f"[dogeing] Building {cfg.project_name} [{arch}]")
        print(f"[dogeing] Found {len(c_source)} Kernel C files, {len(asm_source)} Kernel ASM files, {len(app_sources)} App files, {len(package_sources)} Package files")

        if not args.skip_analyze:
            print("[0/4] Running Static Analyzer (parallel)...")
            analyzer_bin = tools.get("analyzer", tools["c_compiler"])
            analyze_flag = "-fanalyzer -fsyntax-only" if "gcc" in analyzer_bin else "--analyze"
            analyzer_output = (
                "" if "gcc" in analyzer_bin
                else " -Xanalyzer -analyzer-output=text"
            )
            analyze_cmds = []
            for src in c_source:
                analyze_cmds.append(
                    f"{analyzer_bin} {analyze_flag} {arch_cfg['kernel_flags']} "
                    f"{common_flags}{analyzer_output} {src}"
                )
            analyze_cmds += [
                f"{analyzer_bin} {analyze_flag} {arch_cfg['app_flags']} "
                f"{common_flags}{analyzer_output} {src}"
                for src, ftype in app_sources + package_sources if ftype == "c"
            ]
            for lib_src in app_library_paths:
                if lib_src.endswith(".c"):
                    lib_flags = " -Wno-conversion" if os.path.basename(lib_src) == "chasm.c" else ""
                    analyze_cmds.append(
                        f"{analyzer_bin} {analyze_flag} {arch_cfg['app_flags']} "
                        f"{common_flags}{analyzer_output}{lib_flags} {lib_src}"
                    )
            if analyze_cmds:
                run_parallel(analyze_cmds)

        print("[1/4] Compiling Kernel Sources (parallel)...")
        compile_cmds = []
        object_files = []

        for src in c_source:
            obj = src.replace(os.sep, "_").replace(".c", ".o")
            object_files.append(obj)
            compile_cmds.append(f"{tools['c_compiler']} {arch_cfg['kernel_flags']} {common_flags} -c {src} -o {obj}")

        for src in asm_source:
            obj = src.replace(os.sep, "_").replace(".asm", ".o")
            object_files.append(obj)
            compile_cmds.append(f"{tools['assembler']} -f elf64 -g -F dwarf {src} -o {obj}")

        run_parallel(compile_cmds)

        objects_str = " ".join(object_files)

        print("[2/4] Linking Kernel ELF...")
        cmd(f"{tools['linker']} {arch_cfg['linker_flags']} -T {arch_cfg['kernel_linker_script']} {objects_str} -o kernel.elf")

        total_compile_items = app_sources + package_sources
        if total_compile_items:
            print(f"[3/4] Compiling {len(app_sources)} Application(s) and {len(package_sources)} Package(s) (parallel)...")
            app_library_objects = []
            library_compile_cmds = []
            for lib_src in app_library_paths:
                if lib_src.endswith(".c"):
                    lib_obj = lib_src.replace(os.sep, "_").replace(".c", ".o")
                    app_library_objects.append(lib_obj)
                    lib_flags = " -Wno-conversion" if os.path.basename(lib_src) == "chasm.c" else ""
                    library_compile_cmds.append(
                        f"{tools['c_compiler']} {arch_cfg['app_flags']} {common_flags}"
                        f"{lib_flags} -c {lib_src} -o {lib_obj}"
                    )
            run_parallel(library_compile_cmds)

            app_cmds = []
            for app_src, ftype in app_sources:
                ext = ".c" if ftype == "c" else ".asm"
                app_obj = app_src.replace(ext, ".o")
                binary_name = getattr(cfg, "app_binary_names", {}).get(
                    os.path.normpath(os.path.relpath(app_src)),
                    os.path.splitext(os.path.basename(app_src))[0],
                )
                app_elf = os.path.join(os.path.dirname(app_src), binary_name + ".elf")

                system_apps_dir = os.path.abspath(
                    os.path.join("apps", "system")
                )

                normalized_app_path = os.path.abspath(
                    os.path.normpath(app_src)
                )

                if normalized_app_path.startswith(system_apps_dir + os.sep):
                    compiled_system_app_elfs.append(app_elf)
                else:
                    compiled_app_elfs.append(app_elf)

                if ftype == "c":
                    compile_step = f"{tools['c_compiler']} {arch_cfg['app_flags']} {common_flags} -c {app_src} -o {app_obj}"
                else:
                    compile_step = f"{tools['assembler']} -f elf64 -g -F dwarf {app_src} -o {app_obj}"

                linker_inputs = f"{app_obj} {' '.join(app_library_objects)}"
                linker_args = (
                    f"{tools['linker']} {arch_cfg['linker_flags']} "
                    f"--no-relax --gc-sections"
                )
                elf_linker_script = arch_cfg.get(
                    "app_elf_linker_script", arch_cfg["app_linker_script"]
                )
                chain_cmd = (
                    f"{compile_step} && "
                    f"{linker_args} -T {elf_linker_script} "
                    f"{linker_inputs} -o {app_elf} && "
                    f"rm -f {app_obj}"
                )
                app_cmds.append(chain_cmd)

            for pkg_src, ftype in package_sources:
                ext = ".c" if ftype == "c" else ".asm"
                pkg_obj = pkg_src.replace(ext, ".o")
                binary_name = os.path.splitext(os.path.basename(pkg_src))[0]
                pkg_elf = os.path.join(os.path.dirname(pkg_src), binary_name + ".elf")
                pkg_lz4 = os.path.join(os.path.dirname(pkg_src), binary_name + ".lz4")
                compressed_packages.append(pkg_lz4)

                if ftype == "c":
                    compile_step = f"{tools['c_compiler']} {arch_cfg['app_flags']} {common_flags} -c {pkg_src} -o {pkg_obj}"
                else:
                    compile_step = f"{tools['assembler']} -f elf64 -g -F dwarf {pkg_src} -o {pkg_obj}"

                def compress_file_task(elf_path, lz4_path):
                    with open(elf_path, "rb") as f_in:
                        compressed_data = lz4_compress_data(f_in.read())
                    with open(lz4_path, "wb") as f_out:
                        f_out.write(compressed_data)

                package_elf_linker_script = arch_cfg.get(
                    "app_elf_linker_script", arch_cfg["app_linker_script"]
                )
                link_and_copy_cmd = (
                    f"{compile_step} && "
                    f"{tools['linker']} {arch_cfg['linker_flags']} --no-relax --gc-sections "
                    f"-T {package_elf_linker_script} {pkg_obj} {' '.join(app_library_objects)} -o {pkg_elf}"
                )
                
                def build_package():
                    success, _, stdout, stderr = run_cmd(link_and_copy_cmd)
                    print_command_output(stdout, stderr)
                    if not success:
                        return False, pkg_src
                    try:
                        compress_file_task(pkg_elf, pkg_lz4)
                        if os.path.exists(pkg_obj): os.remove(pkg_obj)
                        if os.path.exists(pkg_elf): os.remove(pkg_elf)
                        return True, pkg_src
                    except (OSError, ValueError) as error:
                        return False, f"{pkg_src}: {error}"

                success, failed_item = build_package()
                if not success:
                    print(f"\n[Not Wow] Build failed on package:\n  {failed_item}")
                    sys.exit(1)

            run_parallel(app_cmds)
        else:
            print("[3/4] Warning: No application source files found in source_folders_apps!")

        print("[4/4] Generating Disk Image (FAT Boot + exFAT Data)...")

        img_file = cfg.img_file
        if os.path.exists(img_file):
            os.remove(img_file)

        cmd(f"dd if=/dev/zero bs=1M count={DISK_IMAGE_SIZE_MB} of={img_file} 2>/dev/null")

        partition_type_p1 = "ef00" if boot_mode == "uefi" else "8300"
        
        p2_start = 4096
        total_sectors = (DISK_IMAGE_SIZE_MB * 1024 * 1024) // 512
        p2_end = total_sectors - 2048
        
        partition_command = (
            f"PATH=$PATH:/usr/sbin:/sbin sgdisk {img_file} "
            f"-n 1:2048:4095 -t 1:{partition_type_p1} "
            f"-n 2:{p2_start}:{p2_end} -t 2:0700"
        )
        if boot_mode == "bios":
            partition_command += " -m 1"
        cmd(partition_command + " 2>/dev/null")

        if boot_mode == "bios" and arch == "x86_64" and os.path.exists("binaries/limine"):
            cmd("chmod +x binaries/limine 2>/dev/null || true", critical=False)
            cmd(f"./binaries/limine bios-install {img_file} 2>/dev/null", critical=False)

        boot_offset = 1048576
        esp_image = None
        if boot_mode == "uefi":
            esp_fd, esp_image = tempfile.mkstemp(prefix="windoge-esp-", suffix=".img")
            os.close(esp_fd)
            with open(esp_image, "wb") as esp_file:
                esp_file.truncate(boot_offset)
            boot_volume = esp_image
        else:
            boot_volume = f"{img_file}@@{boot_offset}"

        try:
            cmd(f"mformat -i {boot_volume} -v BOOT")
            cmd(f"mmd -i {boot_volume} ::/boot")
            cmd(f"mmd -i {boot_volume} ::/boot/limine")
            if boot_mode == "uefi":
                cmd(f"mmd -i {boot_volume} ::/EFI")
                cmd(f"mmd -i {boot_volume} ::/EFI/BOOT")

            cmd(f"mcopy -i {boot_volume} kernel.elf ::/boot/kernel.elf")

            if os.path.exists("limine.conf"):
                cmd(f"mcopy -i {boot_volume} limine.conf ::/limine.conf")
                cmd(f"mcopy -i {boot_volume} limine.conf ::/boot/limine/limine.conf")

            if boot_mode == "bios":
                sys_binary = arch_cfg.get("limine_sys", "")
                if os.path.exists(sys_binary):
                    cmd(f"mcopy -i {boot_volume} {sys_binary} ::/boot/limine/limine-bios.sys")
            else:
                uefi_bootloader = arch_cfg.get("uefi_bootloader", "")
                if not uefi_bootloader or not os.path.exists(uefi_bootloader):
                    print(f"[Not Wow] UEFI bootloader not found: {uefi_bootloader}")
                    sys.exit(1)
                efi_filename = "BOOTAA64.EFI" if is_arm64 else "BOOTX64.EFI"
                cmd(
                    f"mcopy -i {boot_volume} {uefi_bootloader} "
                    f"::/EFI/BOOT/{efi_filename}"
                )
                cmd(
                    f"dd if={esp_image} of={img_file} bs=512 "
                    f"seek=2048 conv=notrunc 2>/dev/null"
                )
        finally:
            if esp_image is not None and os.path.exists(esp_image):
                os.remove(esp_image)

        part2_img = "part2_exfat.img"
        p2_sectors = p2_end - p2_start + 1
        cmd(f"dd if=/dev/zero of={part2_img} bs=512 count={p2_sectors} 2>/dev/null")
        
        cmd(f"mkfs.exfat -b 4K -L WINDOGEOS {part2_img}")

        if compiled_app_elfs or compiled_system_app_elfs or compressed_packages:
            mnt_dir = "/tmp/windoge_p2_mnt"
            os.makedirs(mnt_dir, exist_ok=True)

            loop_dev = subprocess.check_output(
                f"sudo losetup -f --show {part2_img}",
                shell=True
            ).decode().strip()

            cmd(f"sudo mount.exfat-fuse {loop_dev} {mnt_dir}")

            try:
                cmd(f"sudo mkdir -p {mnt_dir}/apps")
                cmd(f"sudo mkdir -p {mnt_dir}/apps/packages")
                cmd(f"sudo mkdir -p {mnt_dir}/apps/text")
                cmd(f"sudo mkdir -p {mnt_dir}/system")

                for app_elf in compiled_app_elfs:
                    if os.path.exists(app_elf):
                        elf_name = os.path.basename(app_elf)
                        cmd(
                            f"sudo cp '{app_elf}' "
                            f"'{mnt_dir}/apps/{elf_name}'"
                        )

                for app_elf in compiled_system_app_elfs:
                    if os.path.exists(app_elf):
                        elf_name = os.path.basename(app_elf)
                        cmd(
                            f"sudo cp '{app_elf}' "
                            f"'{mnt_dir}/system/{elf_name}'"
                        )

                for pkg_lz4 in compressed_packages:
                    if os.path.exists(pkg_lz4):
                        pkg_name = os.path.basename(pkg_lz4)

                        cmd(
                            f"sudo cp '{pkg_lz4}' "
                            f"'{mnt_dir}/apps/packages/{pkg_name}'"
                        )

                        print(
                            f"[dogeing] Injected package "
                            f"to exFAT /apps/packages: {pkg_name}"
                        )

                #
                # Assets -> /apps/text
                #
                assets_source_dir = os.path.join("apps", "text")

                if os.path.isdir(assets_source_dir):
                    for root, _, files in os.walk(assets_source_dir):

                        rel_dir = os.path.relpath(
                            root,
                            assets_source_dir
                        )

                        if rel_dir == ".":
                            dst_dir = os.path.join(
                                mnt_dir,
                                "apps",
                                "text"
                            )
                        else:
                            dst_dir = os.path.join(
                                mnt_dir,
                                "apps",
                                "text",
                                rel_dir
                            )

                        cmd(f"sudo mkdir -p '{dst_dir}'")

                        for file in files:
                            src_file = os.path.join(root, file)
                            dst_file = os.path.join(dst_dir, file)

                            cmd(
                                f"sudo cp '{src_file}' "
                                f"'{dst_file}'"
                            )

                            rel_path = os.path.relpath(
                                src_file,
                                assets_source_dir
                            )

                            print(
                                f"[dogeing] Injected asset "
                                f"to exFAT /apps/text/{rel_path}"
                            )

            finally:
                cmd(f"sudo umount {mnt_dir}")
                cmd(f"sudo losetup -d {loop_dev}")

                if os.path.exists(mnt_dir):
                    os.rmdir(mnt_dir)

        cmd(f"dd if={part2_img} of={img_file} bs=512 seek={p2_start} conv=notrunc 2>/dev/null")

        elapsed = time.perf_counter() - start_time
        
        cmd("find . -name \"*.elf\" -type f -delete")
        cmd("find . -name \"*.lz4\" -type f -delete")

        print(f"\n[dogeing] Build success, doesn't mean it will work >:)")
        print(f"[dogeing] Output generated: {img_file}")
        print(f"[dogeing] Total build time: {elapsed:.2f} seconds ({elapsed / 60:.2f} mins)")

    finally:
        cleanup()

if __name__ == "__main__":
    main()