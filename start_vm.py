import subprocess
import os
import platform
import argparse

img_file = "windoge_bliss.img"

parser = argparse.ArgumentParser(description="WindogeOS runner, virtual machine runner utility")
parser.add_argument("-a", "--arch", default="x86_64", help="CPU architecture choice, if there is nothing then it's defaultly x86_64")
args = parser.parse_args()

if not os.path.exists(img_file):
    print(f"Error: Disk image '{img_file}' not found. Please compile it first.")
    exit(1)

print(f"Launching QEMU virtual machine for {args.arch}...")

if args.arch == "x86_64":
    qemu_cmd = f"qemu-system-x86_64 -hda {img_file} -M q35 -m 256M"
elif args.arch in ["arm64", "aarch64"]:
    qemu_cmd = f"qemu-system-aarch64 -M virt -cpu cortex-a57 -m 256M -bios limine-binary/BOOTAA64.EFI -drive file={img_file},if=none,id=drv0 -device virtio-blk-device,drive=drv0"
else:
    print("Unknown architecture! Only x86_64 or arm64 (or aarch64) are supported.")
    exit(1)

subprocess.run(qemu_cmd, shell=True)
