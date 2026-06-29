import subprocess
import shutil
import os
import platform
import argparse

cc_base = "clang"
cpu = "x86_64"
img_file = "windoge_bliss.img"
system = platform.system()

if system == "Windows":
    linker = "ld.lld"
else:
    linker = "ld"

parser = argparse.ArgumentParser(description="WindogeOS compiler, virtual machine not included")
parser.add_argument("-a", "--arch", default="x86_64", help="CPU architecture choice, if there is nothing then it's defaultly x86_64")
args = parser.parse_args()

if args.arch == "x86_64":
    cc = "clang -target x86_64-unknown-none-elf -Wall -Wextra -std=gnu11 -nostdinc -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections -Iheaders -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel"
    linker_file = "linker/linker_x86_64.ld"
    linker_flags = "-m elf_x86_64"
    
elif args.arch == "arm64" or args.arch == "aarch64":
    cc = "clang -target aarch64-unknown-none-elf -Wall -Wextra -std=gnu11 -nostdinc -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fno-PIC -ffunction-sections -fdata-sections -Iheaders -mcpu=generic -march=armv8-a+nofp+nosimd -mgeneral-regs-only"
    linker_file = "linker/linker_arm64.ld"
    linker_flags = "-m aarch64elf"
else:
    print("unknown architecture, only x86_64 or arm64 (or aarch64) supported,")
    print("leave empty for default (x86_64)")
    exit(1)

search_directories = ["kernel", "helper", "libs", "drivers"]
source_files = []
object_files = []
compile_lines = []

for folder in search_directories:
    if os.path.exists(folder):
        for root, dirs, files in os.walk(folder):
            for file in files:
                if file.endswith(".c"):
                    source_path = os.path.join(root, file)
                    flat_object_name = source_path.replace(os.sep, "_").replace(".c", ".o")
                    source_files.append(source_path)
                    object_files.append(flat_object_name)
                    compile_lines.append(f"{cc} -c {source_path} -o {flat_object_name}")

if not source_files:
    print("Error: No source files found to build.")
    exit(1)

objects_string = " ".join(object_files)
compile_commands_string = "\n".join(compile_lines)

src_compile_script = f"""
set -e
{compile_commands_string}
{linker} {linker_flags} -T {linker_file} {objects_string} -o kernel.elf
"""

create_img_script_x86_64 = f"""
set -e
rm -f {img_file};
dd if=/dev/zero bs=1M count=0 seek=64 of={img_file};
PATH=$PATH:/usr/sbin:/sbin sgdisk {img_file} -n 1:2048 -t 1:ef00 -m 1;
./limine-binary/limine bios-install {img_file};
mformat -i {img_file}@@1M;
mmd -i {img_file}@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine;
mcopy -i {img_file}@@1M kernel.elf ::/boot;
mcopy -i {img_file}@@1M limine.conf ::/boot/limine;
mcopy -i {img_file}@@1M limine-binary/limine-bios.sys ::/boot/limine;
mcopy -i {img_file}@@1M limine-binary/BOOTX64.EFI ::/EFI/BOOT;
mcopy -i {img_file}@@1M limine-binary/BOOTIA32.EFI ::/EFI/BOOT;
rm -f {objects_string} kernel.elf
mcopy -o -i windoge_bliss.img@@1M files/readme.txt ::/readme.txt
mdir -i windoge_bliss.img@@1M ::/
"""

create_img_script_arm64 = f"""
set -e
rm -f {img_file};
dd if=/dev/zero bs=1M count=0 seek=64 of={img_file};
PATH=$PATH:/usr/sbin:/sbin sgdisk {img_file} -n 1:2048 -t 1:ef00;
mformat -i {img_file}@@1M;
mmd -i {img_file}@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine;
mcopy -i {img_file}@@1M kernel.elf ::/boot;
mcopy -i {img_file}@@1M limine.conf ::/boot/limine;
mcopy -i {img_file}@@1M limine-binary/BOOTAA64.EFI ::/EFI/BOOT;
rm -f {objects_string} kernel.elf
mcopy -o -i windoge_bliss.img@@1M files/readme.txt ::/readme.txt
mdir -i windoge_bliss.img@@1M ::/
"""

try:
    if args.arch == "x86_64":
        subprocess.run(src_compile_script, shell=True, check=True, capture_output=True, text=True)
        subprocess.run(create_img_script_x86_64, shell=True, check=True)
    elif args.arch == "arm64" or args.arch == "aarch64":
        subprocess.run(src_compile_script, shell=True, check=True, capture_output=True, text=True)
        subprocess.run(create_img_script_arm64, shell=True, check=True)
    print("Build successful.")
except subprocess.CalledProcessError as e:
    print("\n--- BUILD FAILED ---")
    if e.stdout:
        print("STDOUT:\n", e.stdout)
    if e.stderr:
        print("STDERR:\n", e.stderr)
    exit(1)
