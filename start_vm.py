import subprocess
import sys
import os
import platform
import argparse
import shlex

img_file = "windoge_os.img"

parser = argparse.ArgumentParser(description="WindogeOS runner, virtual machine runner utility")
parser.add_argument("-a", "--arch", default="x86_64", help="CPU architecture choice, defaults to x86_64")
parser.add_argument("-d", "--debug", action="store_true", help="Enable QEMU interrupt logging and dump to qemu.log")
parser.add_argument("--secondary-disk", help="Attach a raw secondary IDE disk image (x86_64 only)")
args = parser.parse_args()

if not os.path.exists(img_file):
    print(f"Error: Disk image '{img_file}' not found. Please compile it first.")
    sys.exit(1)

if args.secondary_disk and not os.path.isfile(args.secondary_disk):
    print(f"Error: Secondary disk image '{args.secondary_disk}' does not exist.")
    sys.exit(1)

print("--------------------------- ")
print("Starting WindogeOS in QEMU |")
print("--------------------------- ")

debug_flags = "-d int,cpu_reset -no-reboot -D qemu.log" if args.debug else ""

if args.arch == "x86_64":
    qemu_cmd = f"qemu-system-x86_64 -cpu Conroe,+lm -rtc base=localtime -M pc -m 6M -hda {img_file} -serial stdio {debug_flags}"
    if args.secondary_disk:
        qemu_cmd += f" -hdb {shlex.quote(args.secondary_disk)}"
elif args.arch in ["arm64", "aarch64"]:
    if args.secondary_disk:
        print("Error: Secondary IDE disks are only supported on x86_64.")
        sys.exit(1)
    qemu_cmd = f"qemu-system-aarch64 -rtc base=localtime -M virt -cpu cortex-a57 -m 6M -bios limine-binary/BOOTAA64.EFI -drive format=raw,file={img_file},if=none,id=drv0 -device virtio-blk-device,drive=drv0 -serial stdio {debug_flags}"
else:
    print("Unknown architecture! Only x86_64 or arm64 (or aarch64) are supported.")
    sys.exit(1)

subprocess.run(qemu_cmd, shell = True)
