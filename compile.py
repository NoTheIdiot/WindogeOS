import subprocess
import shutil
import os
import platform
import argparse
import glob

img_file = "windoge_os.img"
system = platform.system()

linker = "ld.lld"

parser = argparse.ArgumentParser(description="WindogeOS Dual-Partition Build Script.")
parser.add_argument("-a", "--arch", default="x86_64", help="CPU architecture (default: x86_64)")
args = parser.parse_args()

subprocess.run("clear" if system != "Windows" else "cls", shell=True)

app_linker_file = "linker-files/apps.ld"

if args.arch == "x86_64":
    cc = (
        "clang -target x86_64-unknown-none-elf -Wall -Wextra -Werror -Wconversion "
        "-std=gnu11 -nostdinc -ffreestanding -fno-stack-protector -fno-stack-check "
        "-fno-lto -fno-PIC -fno-pie -ffunction-sections -fdata-sections -Iheaders -m64 "
        "-march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone "
        "-mcmodel=kernel"
    )
    cc_app = (
        "clang -target x86_64-unknown-none-elf -Wall -Wextra -Werror -Wconversion "
        "-std=gnu11 -nostdinc -ffreestanding -fno-stack-protector -fno-stack-check "
        "-fno-lto -fno-PIC -fno-pie -ffunction-sections -fdata-sections -Iheaders -m64 "
        "-march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone "
        "-mcmodel=large -DWINDOGE_APP"
    )
    linker_file = "linker-files/x86_64.ld"
    linker_flags = "-m elf_x86_64"
    
elif args.arch in ["arm64", "aarch64"]:
    cc = (
        "clang -target aarch64-unknown-none-elf -Wall -Wextra -Werror -Wconversion "
        "-std=gnu11 -nostdinc -ffreestanding -fno-stack-protector -fno-stack-check "
        "-fno-lto -fno-PIC -fno-pie -ffunction-sections -fdata-sections -Iheaders "
        "-mcpu=generic -march=armv8-a -mgeneral-regs-only"
    )
    cc_app = (
        "clang -target aarch64-unknown-none-elf -Wall -Wextra -Werror -Wconversion "
        "-std=gnu11 -nostdinc -ffreestanding -fno-stack-protector -fno-stack-check "
        "-fno-lto -fno-PIC -ffunction-sections -fdata-sections -Iheaders "
        "-mcpu=generic -march=armv8-a"
    )
    linker_file = "linker-files/arm64.ld"
    linker_flags = "-m aarch64elf"
else:
    print("Unknown architecture.")
    exit(1)

# Search kernel source files
search_directories = ["kernel", "helper", "libs", "drivers"]
source_files = []
object_files = []
compile_lines = []

for folder in search_directories:
    if os.path.exists(folder):
        for root, dirs, files in os.walk(folder):
            for file in files:
                source_path = os.path.join(root, file)
                if file.endswith(".c"):
                    flat_object_name = source_path.replace(os.sep, "_").replace(".c", ".o")
                    source_files.append(source_path)
                    object_files.append(flat_object_name)
                    compile_lines.append(f"{cc} -c {source_path} -o {flat_object_name}")
                elif file.endswith(".asm"):
                    flat_object_name = source_path.replace(os.sep, "_").replace(".asm", ".o")
                    source_files.append(source_path)
                    object_files.append(flat_object_name)
                    compile_lines.append(f"nasm -f elf64 {source_path} -o {flat_object_name}")

if not source_files:
    print("Error: No source files found to build.")
    exit(1)

objects_str = " ".join(object_files)
compile_commands_str = "\n".join(compile_lines)

src_compile_script = f"""
set -e
{compile_commands_str}
{linker} {linker_flags} -T {linker_file} {objects_str} -o kernel.elf
"""

try:
    print("Compiling kernel source code...")
    subprocess.run(src_compile_script, shell=True, check=True, text=True)

    if os.path.exists("apps"):
        print("Compiling userland flat binaries...")
        for root, _, files in os.walk("apps"):
            for file in files:
                if file.endswith(".c"):
                    app_src = os.path.join(root, file)
                    app_obj = app_src.replace(".c", ".o")
                    app_bin = app_src.replace(".c", ".bin")
                    
                    subprocess.run(f"{cc_app} -c {app_src} -o {app_obj}", shell=True, check=True)
                    subprocess.run(
    f"{linker} {linker_flags} --no-relax -T {app_linker_file} "
    f"--oformat binary --just-symbols=kernel.elf {app_obj} -o {app_bin}",
    shell=True,
    check=True
)
                    os.remove(app_obj)

    # Search for compiled app binaries AFTER compilation completes
    app_copy_commands = ""
    app_clean_files = []
    if os.path.exists("apps"):
        app_bins = glob.glob("apps/**/*.bin", recursive=True) + glob.glob("apps/*.bin")
        app_bins = list(set(app_bins))  # Remove potential duplicates
        for app in app_bins:
            app_copy_commands += f"sudo cp {app} $MOUNT_DATA/;\n"
            app_clean_files.append(app)

    create_img_script_x86_64 = f"""
set -e
rm -f {img_file};
dd if=/dev/zero bs=1M count=0 seek=1028 of={img_file};

# Part 1: Boot (FAT12, LBA 2048-4095). Part 2: exFAT Data (LBA 4096+)
PATH=$PATH:/usr/sbin:/sbin sgdisk {img_file} \
  -n 1:2048:4095 -t 1:8300 \
  -n 2:4096:2101247 -t 2:0700 -m 1;

./binaries/limine bios-install {img_file};

LOOP_BOOT=$(sudo losetup -f --show -o 1048576 --sizelimit 1048576 {img_file})
LOOP_DATA=$(sudo losetup -f --show -o 2097152 {img_file})

# Format Partition 1 as FAT12 (no 1MB structure padding, 512b sectors)
sudo mkfs.fat -F 12 -a -s 1 -n "BOOT" $LOOP_BOOT
sudo mkfs.exfat -s 512 -c 4K -n "WINDOGEOS" $LOOP_DATA

# Mount Boot Partition and copy ONLY BIOS components + Kernel
MOUNT_BOOT=$(mktemp -d)
sudo mount $LOOP_BOOT $MOUNT_BOOT
sudo mkdir -p $MOUNT_BOOT/boot/limine
sudo cp kernel.elf $MOUNT_BOOT/boot/
sudo cp limine.conf $MOUNT_BOOT/
sudo cp limine.conf $MOUNT_BOOT/boot/limine/
sudo cp binaries/limine-bios.sys $MOUNT_BOOT/boot/limine/
sudo umount $MOUNT_BOOT
sudo losetup -d $LOOP_BOOT
rmdir $MOUNT_BOOT

# Mount exFAT Data Partition and copy apps
MOUNT_DATA=$(mktemp -d)
sudo mount -t exfat-fuse $LOOP_DATA $MOUNT_DATA 2>/dev/null || sudo mount $LOOP_DATA $MOUNT_DATA
{app_copy_commands}
sudo umount $MOUNT_DATA
sudo losetup -d $LOOP_DATA
rmdir $MOUNT_DATA

rm -f {objects_str} kernel.elf {" ".join(app_clean_files)}
"""

    create_img_script_arm64 = f"""
set -e
rm -f {img_file};
dd if=/dev/zero bs=1M count=0 seek=1028 of={img_file};

# Part 1: Boot (FAT16/EFI, LBA 2048-4095). Part 2: exFAT Data (LBA 4096+)
PATH=$PATH:/usr/sbin:/sbin sgdisk {img_file} \
  -n 1:2048:4095 -t 1:ef00 \
  -n 2:4096:2101247 -t 2:0700 -m 1;

LOOP_BOOT=$(sudo losetup -f --show -o 1048576 --sizelimit 1048576 {img_file})
LOOP_DATA=$(sudo losetup -f --show -o 2097152 {img_file})

# Format Partition 1 as FAT16 for UEFI compatibility
sudo mkfs.fat -F 16 -n "BOOT" $LOOP_BOOT
sudo mkfs.exfat -s 512 -c 4K -n "WINDOGEOS" $LOOP_DATA

# Mount Boot Partition and copy UEFI Limine + Kernel
MOUNT_BOOT=$(mktemp -d)
sudo mount $LOOP_BOOT $MOUNT_BOOT
sudo mkdir -p $MOUNT_BOOT/EFI/BOOT $MOUNT_BOOT/boot/limine
sudo cp kernel.elf $MOUNT_BOOT/boot/
sudo cp limine.conf $MOUNT_BOOT/
sudo cp limine.conf $MOUNT_BOOT/boot/limine/
sudo cp binaries/limine-uefi-aarch64.efi $MOUNT_BOOT/EFI/BOOT/BOOTAA64.EFI 2>/dev/null || true
sudo umount $MOUNT_BOOT
sudo losetup -d $LOOP_BOOT
rmdir $MOUNT_BOOT

# Mount exFAT Data Partition and copy apps
MOUNT_DATA=$(mktemp -d)
sudo mount -t exfat-fuse $LOOP_DATA $MOUNT_DATA 2>/dev/null || sudo mount $LOOP_DATA $MOUNT_DATA
{app_copy_commands}
sudo umount $MOUNT_DATA
sudo losetup -d $LOOP_DATA
rmdir $MOUNT_DATA

rm -f {objects_str} kernel.elf {" ".join(app_clean_files)}
"""

    print("Generating dual-partition image (FAT12 Boot + LBA-4096 exFAT Data)...")
    if args.arch == "x86_64":
        subprocess.run(create_img_script_x86_64, shell=True, check=True)
    elif args.arch in ["arm64", "aarch64"]:
        subprocess.run(create_img_script_arm64, shell=True, check=True)

    print("Build success, doesn't mean it will work >:)")
    exit(0)

except subprocess.CalledProcessError as e:
    print("\n-------Build failed--------")
    subprocess.run(f"rm -f {objects_str} kernel.elf", shell=True)
    exit(1)