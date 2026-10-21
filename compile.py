import os
import sys
import time
import argparse
import subprocess
import importlib.util
from concurrent.futures import ProcessPoolExecutor, as_completed

DISK_IMAGE_SIZE_MB = 6

def rle_compress_data(data: bytes) -> bytes:
    original_size = len(data)
    compressed = bytearray()
    compressed.extend(original_size.to_bytes(4, byteorder='little'))
    src_idx = 0
    src_size = original_size
    while src_idx < src_size:
        run_len = 1
        current_val = data[src_idx]
        while src_idx + run_len < src_size and run_len < 128:
            if data[src_idx + run_len] != current_val:
                break
            run_len += 1
        if run_len >= 2:
            ctrl = (-(run_len - 1)) & 0xFF
            compressed.append(ctrl)
            compressed.append(current_val)
            src_idx += run_len
        else:
            lit_buf = bytearray()
            lit_len = 0
            while src_idx + lit_len < src_size and lit_len < 128:
                if src_idx + lit_len + 1 < src_size:
                    if data[src_idx + lit_len] == data[src_idx + lit_len + 1]:
                        break
                lit_buf.append(data[src_idx + lit_len])
                lit_len += 1
            if lit_len > 0:
                ctrl = (lit_len - 1) & 0xFF
                compressed.append(ctrl)
                compressed.extend(lit_buf)
                src_idx += lit_len
    return bytes(compressed)

def cmd(cmd_str, critical=True):
    res = subprocess.run(cmd_str, shell=True)
    if res.returncode != 0 and critical:
        print(f"\n[Not Wow] Build failed on command:\n  {cmd_str}")
        sys.exit(1)
    return res.returncode == 0

def run_cmd(cmd_str):
    res = subprocess.run(cmd_str, shell=True)
    return res.returncode == 0, cmd_str

def run_parallel(commands):
    if not commands:
        return
    with ProcessPoolExecutor() as executor:
        futures = [executor.submit(run_cmd, c) for c in commands]
        for future in as_completed(futures):
            success, failed_cmd = future.result()
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
        config_path = "compile_config.py"
        if not os.path.exists(config_path):
            print(f"[Not Wow] Could not find configuration file '{config_path}'")
            sys.exit(1)

        spec = importlib.util.spec_from_file_location("compile_config", config_path)
        cfg = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cfg)

        parser = argparse.ArgumentParser(description=f"{cfg.project_name} Dual-Partition Build Engine")
        parser.add_argument("-a", "--arch", default=cfg.default_arch, help="Target architecture (default: %(default)s)")
        parser.add_argument("--skip-analyze", action="store_true", help="Skip running Clang Static Analyzer")
        args = parser.parse_args()

        arch = args.arch
        if arch not in cfg.compiler_flags:
            print(f"[Not Wow] Unknown architecture '{arch}'. Supported options: {list(cfg.compiler_flags.keys())}")
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

        app_sources = []
        package_sources = []
        
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
                        if normalized_path in app_library_paths:
                            continue
                        relative_path = os.path.normpath(os.path.relpath(file_path))
                        if arch not in app_source_arches.get(relative_path, [arch]):
                            continue
                        
                        is_package = "apps" + os.sep + "packages" in normalized_path or "userland" + os.sep + "packages" in normalized_path
                        
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
            analyze_cmds = [
                f"{analyzer_bin} {analyze_flag} {arch_cfg['kernel_flags']} {common_flags} {src}"
                for src in c_source
            ] + [
                f"{analyzer_bin} {analyze_flag} {arch_cfg['app_flags']} {common_flags} {src}"
                for src, ftype in app_sources + package_sources if ftype == "c"
            ]
            for lib_src in app_library_paths:
                if lib_src.endswith(".c"):
                    lib_flags = " -Wno-conversion" if os.path.basename(lib_src) == "chasm.c" else ""
                    analyze_cmds.append(
                        f"{analyzer_bin} {analyze_flag} {arch_cfg['app_flags']} "
                        f"{common_flags}{lib_flags} {lib_src}"
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

        compiled_apps = []
        compressed_packages = []

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
                app_bin = os.path.join(os.path.dirname(app_src), binary_name + ".bin")
                compiled_apps.append(app_bin)

                if ftype == "c":
                    compile_step = f"{tools['c_compiler']} {arch_cfg['app_flags']} {common_flags} -c {app_src} -o {app_obj}"
                else:
                    compile_step = f"{tools['assembler']} -f elf64 -g -F dwarf {app_src} -o {app_obj}"

                chain_cmd = (
                    f"{compile_step} && "
                    f"{tools['linker']} {arch_cfg['linker_flags']} --no-relax --gc-sections "
                    f"-T {arch_cfg['app_linker_script']} {app_obj} {' '.join(app_library_objects)} -o {app_elf} && "
                    f"{tools['objcopy']} -O binary {app_elf} {app_bin} && "
                    f"rm -f {app_obj} {app_elf}"
                )
                app_cmds.append(chain_cmd)

            for pkg_src, ftype in package_sources:
                ext = ".c" if ftype == "c" else ".asm"
                pkg_obj = pkg_src.replace(ext, ".o")
                binary_name = os.path.splitext(os.path.basename(pkg_src))[0]
                pkg_elf = os.path.join(os.path.dirname(pkg_src), binary_name + ".elf")
                pkg_bin = os.path.join(os.path.dirname(pkg_src), binary_name + ".bin")
                pkg_rle = os.path.join(os.path.dirname(pkg_src), binary_name + ".rle")
                compressed_packages.append(pkg_rle)

                if ftype == "c":
                    compile_step = f"{tools['c_compiler']} {arch_cfg['app_flags']} {common_flags} -c {pkg_src} -o {pkg_obj}"
                else:
                    compile_step = f"{tools['assembler']} -f elf64 -g -F dwarf {pkg_src} -o {pkg_obj}"

                def compress_file_task(bin_path, rle_path):
                    with open(bin_path, "rb") as f_in:
                        compressed_data = rle_compress_data(f_in.read())
                    with open(rle_path, "wb") as f_out:
                        f_out.write(compressed_data)

                link_and_copy_cmd = (
                    f"{compile_step} && "
                    f"{tools['linker']} {arch_cfg['linker_flags']} --no-relax --gc-sections "
                    f"-T {arch_cfg['app_linker_script']} {pkg_obj} {' '.join(app_library_objects)} -o {pkg_elf} && "
                    f"{tools['objcopy']} -O binary {pkg_elf} {pkg_bin}"
                )
                
                def build_package():
                    success, _ = run_cmd(link_and_copy_cmd)
                    if not success:
                        return False, pkg_src
                    try:
                        compress_file_task(pkg_bin, pkg_rle)
                        if os.path.exists(pkg_obj): os.remove(pkg_obj)
                        if os.path.exists(pkg_elf): os.remove(pkg_elf)
                        if os.path.exists(pkg_bin): os.remove(pkg_bin)
                        return True, pkg_src
                    except Exception as e:
                        return False, pkg_src

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

        partition_type_p1 = "8300" if arch == "x86_64" else "ef00"
        
        p2_start = 4096
        total_sectors = (DISK_IMAGE_SIZE_MB * 1024 * 1024) // 512
        p2_end = total_sectors - 2048
        
        cmd(
            f"PATH=$PATH:/usr/sbin:/sbin sgdisk {img_file} "
            f"-n 1:2048:4095 -t 1:{partition_type_p1} "
            f"-n 2:{p2_start}:{p2_end} -t 2:0700 -m 1 2>/dev/null"
        )

        if arch == "x86_64" and os.path.exists("binaries/limine"):
            cmd("chmod +x binaries/limine 2>/dev/null || true", critical=False)
            cmd(f"./binaries/limine bios-install {img_file} 2>/dev/null", critical=False)

        boot_offset = 1048576

        cmd(f"mformat -i {img_file}@@{boot_offset} -v BOOT")
        cmd(f"mmd -i {img_file}@@{boot_offset} ::/boot")
        cmd(f"mmd -i {img_file}@@{boot_offset} ::/boot/limine")
        if arch in ["arm64", "aarch64"]:
            cmd(f"mmd -i {img_file}@@{boot_offset} ::/EFI")
            cmd(f"mmd -i {img_file}@@{boot_offset} ::/EFI/BOOT")

        cmd(f"mcopy -i {img_file}@@{boot_offset} kernel.elf ::/boot/kernel.elf")

        if os.path.exists("limine.conf"):
            cmd(f"mcopy -i {img_file}@@{boot_offset} limine.conf ::/limine.conf")
            cmd(f"mcopy -i {img_file}@@{boot_offset} limine.conf ::/boot/limine/limine.conf")

        sys_binary = arch_cfg.get("limine_sys", "")
        if os.path.exists(sys_binary):
            if arch == "x86_64":
                cmd(f"mcopy -i {img_file}@@{boot_offset} {sys_binary} ::/boot/limine/limine-bios.sys")
            elif arch in ["arm64", "aarch64"]:
                cmd(f"mcopy -i {img_file}@@{boot_offset} {sys_binary} ::/EFI/BOOT/BOOTAA64.EFI")

        part2_img = "part2_exfat.img"
        p2_sectors = p2_end - p2_start + 1
        cmd(f"dd if=/dev/zero of={part2_img} bs=512 count={p2_sectors} 2>/dev/null")
        
        cmd(f"mkfs.exfat -b 4K -L WINDOGEOS {part2_img}")

        if compiled_apps or compressed_packages:
            mnt_dir = "/tmp/windoge_p2_mnt"
            os.makedirs(mnt_dir, exist_ok=True)
            loop_dev = subprocess.check_output(f"sudo losetup -f --show {part2_img}", shell=True).decode().strip()
            cmd(f"sudo mount.exfat-fuse {loop_dev} {mnt_dir}")
            try:
                cmd(f"sudo mkdir -p {mnt_dir}/apps")
                cmd(f"sudo mkdir -p {mnt_dir}/apps/packages")
                
                for app_bin in compiled_apps:
                    if os.path.exists(app_bin):
                        app_name = os.path.basename(app_bin)
                        cmd(f"sudo cp {app_bin} {mnt_dir}/apps/{app_name}")
                        print(f"[dogeing] Injected binary to exFAT /apps: {app_name}")

                for pkg_rle in compressed_packages:
                    if os.path.exists(pkg_rle):
                        pkg_name = os.path.basename(pkg_rle)
                        cmd(f"sudo cp {pkg_rle} {mnt_dir}/apps/packages/{pkg_name}")
                        print(f"[dogeing] Injected RLE package to exFAT /apps/packages: {pkg_name}")
            finally:
                cmd(f"sudo umount {mnt_dir}")
                cmd(f"sudo losetup -d {loop_dev}")
                if os.path.exists(mnt_dir):
                    os.rmdir(mnt_dir)

        cmd(f"dd if={part2_img} of={img_file} bs=512 seek={p2_start} conv=notrunc 2>/dev/null")

        elapsed = time.perf_counter() - start_time

        print(f"\n[dogeing] Build success, doesn't mean it will work >:)")
        print(f"[dogeing] Output generated: {img_file}")
        print(f"[dogeing] Total build time: {elapsed:.2f} seconds ({elapsed / 60:.2f} mins)")

    finally:
        cleanup()

if __name__ == "__main__":
    main()