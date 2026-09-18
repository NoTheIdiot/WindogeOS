import os
import sys
import time
import argparse
import subprocess
import importlib.util
from concurrent.futures import ProcessPoolExecutor, as_completed

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

def main():
    start_time = time.perf_counter()

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
        "-Wall -Wextra -Werror -Wconversion -std=gnu11 -nostdinc -ffreestanding "
        "-fno-stack-protector -fno-stack-check -fno-lto -fno-PIC -fno-pie "
        "-ffunction-sections -fdata-sections -Iheaders"
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

    app_c_source = []
    for app_dir in cfg.source_folders_apps:
        if os.path.exists(app_dir):
            for root, _, files in os.walk(app_dir):
                for file in files:
                    if file.endswith(".c"):
                        app_c_source.append(os.path.join(root, file))

    if not c_source and not asm_source:
        print("[Not Wow] No kernel source files found.")
        sys.exit(1)

    print(f"[dogeing] Building {cfg.project_name} [{arch}]")

    if not args.skip_analyze:
        print("[0/4] Running Clang Static Analyzer (parallel)...")
        analyze_cmds = [
            f"{tools['c_compiler']} --analyze {arch_cfg['kernel_flags']} {common_flags} {src}"
            for src in c_source
        ] + [
            f"{tools['c_compiler']} --analyze {arch_cfg['app_flags']} {common_flags} {src}"
            for src in app_c_source
        ]
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
        compile_cmds.append(f"{tools['assembler']} -f elf64 {src} -o {obj}")

    run_parallel(compile_cmds)

    objects_str = " ".join(object_files)

    print("[2/4] Linking Kernel ELF...")
    cmd(f"{tools['linker']} {arch_cfg['linker_flags']} -T {arch_cfg['kernel_linker_script']} {objects_str} -o kernel.elf")

    compiled_apps = []

    if app_c_source:
        print("[3/4] Compiling Applications (parallel)...")
        app_cmds = []
        for app_src in app_c_source:
            app_obj = app_src.replace(".c", ".o")
            app_elf = app_src.replace(".c", ".elf")
            app_bin = app_src.replace(".c", ".bin")
            compiled_apps.append(app_bin)

            chain_cmd = (
                f"{tools['c_compiler']} {arch_cfg['app_flags']} {common_flags} -c {app_src} -o {app_obj} && "
                f"{tools['linker']} {arch_cfg['linker_flags']} --no-relax -T {arch_cfg['app_linker_script']} {app_obj} -o {app_elf} && "
                f"{tools['objcopy']} -O binary {app_elf} {app_bin} && "
                f"rm -f {app_obj} {app_elf}"
            )
            app_cmds.append(chain_cmd)

        run_parallel(app_cmds)

    print("[4/4] Generating Disk Image (mtools)...")

    img_file = cfg.img_file
    if os.path.exists(img_file):
        os.remove(img_file)

    cmd(f"dd if=/dev/zero bs=1M count=0 seek=32 of={img_file} 2>/dev/null")

    partition_type_p1 = "8300" if arch == "x86_64" else "ef00"
    cmd(
        f"PATH=$PATH:/usr/sbin:/sbin sgdisk {img_file} "
        f"-n 1:2048:4095 -t 1:{partition_type_p1} "
        f"-n 2:4096:65502 -t 2:0700 -m 1 2>/dev/null"
    )

    if arch == "x86_64" and os.path.exists("binaries/limine"):
        cmd("chmod +x binaries/limine 2>/dev/null || true", critical=False)
        cmd(f"./binaries/limine bios-install {img_file} 2>/dev/null", critical=False)

    boot_offset = 1048576
    data_offset = 2097152

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

    cmd(f"mformat -i {img_file}@@{data_offset} -F -v WINDOGEOS")

    for app_bin in compiled_apps:
        app_name = os.path.basename(app_bin)
        cmd(f"mcopy -i {img_file}@@{data_offset} {app_bin} ::/{app_name}")

    cmd(f"rm -f {objects_str} kernel.elf")
    cmd("find . -name '*.plist' -type f -delete 2>/dev/null || true", critical=False)

    elapsed = time.perf_counter() - start_time

    print(f"\n[dogeing] Build success, doesn't mean it will work >:)")
    print(f"[dogeing] Output generated: {img_file}")
    print(f"[dogeing] Total build time: {elapsed:.2f} seconds ({elapsed / 60:.2f} mins)")

if __name__ == "__main__":
    main()