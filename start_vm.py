import subprocess
import sys
import os
import argparse
import shlex
import shutil
import tempfile

img_file = "windoge_os.img"

parser = argparse.ArgumentParser(
    description="WindogeOS runner"
)

parser.add_argument(
    "-a",
    "--arch",
    default="x86_64",
    help="Target architecture"
)

parser.add_argument(
    "-d",
    "--debug",
    action="store_true",
    help="Enable QEMU interrupt logging"
)

parser.add_argument(
    "--secondary-disk",
    help="Attach raw secondary IDE disk"
)

parser.add_argument(
    "--uefi",
    action="store_true",
    help="Boot using OVMF"
)

parser.add_argument(
    "--uefi-firmware",
    help="Path to OVMF firmware"
)

args = parser.parse_args()

if not os.path.exists(img_file):
    print(f"Error: '{img_file}' not found.")
    sys.exit(1)

if args.secondary_disk and not os.path.isfile(args.secondary_disk):
    print(
        f"Error: Secondary disk '{args.secondary_disk}' does not exist."
    )
    sys.exit(1)

if args.uefi and args.arch != "x86_64":
    print("Error: --uefi only works on x86_64.")
    sys.exit(1)

if args.uefi_firmware and not args.uefi:
    print("Error: --uefi-firmware requires --uefi.")
    sys.exit(1)

print("---------------------------")
print("Starting WindogeOS in QEMU")
print("---------------------------")

debug_flags = (
    "-d int,cpu_reset -no-reboot -D qemu.log"
    if args.debug
    else ""
)

uefi_vars_copy = None

if args.arch == "x86_64":
    memory = "128M" if args.uefi else "6M"

    qemu_cmd = [
        "qemu-system-x86_64",
        "-cpu", "Conroe,+lm",
        "-rtc", "base=localtime",
        "-machine", "pc",
        "-m", memory,
        "-drive", f"format=raw,file={img_file},if=ide",
        "-serial", "stdio",
    ]

    if debug_flags:
        qemu_cmd.extend(shlex.split(debug_flags))

    if args.uefi:
        firmware = args.uefi_firmware

        if firmware is None:
            for candidate in (
                "/usr/share/OVMF/OVMF.fd",
                "/usr/share/qemu/OVMF.fd",
                "/usr/share/ovmf/OVMF.fd",
            ):
                if os.path.isfile(candidate):
                    firmware = candidate
                    break

        if firmware is not None and os.path.isfile(firmware):
            qemu_cmd.extend([
                "-bios",
                firmware
            ])
        else:
            firmware_pair = None

            for pair in (
                (
                    "/usr/share/OVMF/OVMF_CODE_4M.fd",
                    "/usr/share/OVMF/OVMF_VARS_4M.fd",
                ),
                (
                    "/usr/share/edk2/ovmf/OVMF_CODE.fd",
                    "/usr/share/edk2/ovmf/OVMF_VARS.fd",
                ),
            ):
                if os.path.isfile(pair[0]) and os.path.isfile(pair[1]):
                    firmware_pair = pair
                    break

            if firmware_pair is None:
                print(
                    "Error: Could not locate OVMF firmware."
                )
                sys.exit(1)

            with tempfile.NamedTemporaryFile(
                prefix="windoge-ovmf-vars-",
                suffix=".fd",
                delete=False
            ) as vars_file:
                uefi_vars_copy = vars_file.name

            shutil.copyfile(
                firmware_pair[1],
                uefi_vars_copy
            )

            qemu_cmd.extend([
                "-drive",
                f"if=pflash,format=raw,unit=0,readonly=on,file={firmware_pair[0]}",
                "-drive",
                f"if=pflash,format=raw,unit=1,file={uefi_vars_copy}"
            ])

    if args.secondary_disk:
        qemu_cmd.extend([
            "-drive",
            f"format=raw,file={args.secondary_disk},if=ide"
        ])

elif args.arch in ("arm64", "aarch64"):
    if args.secondary_disk:
        print(
            "Error: secondary IDE disks supported only on x86_64."
        )
        sys.exit(1)

    qemu_cmd = [
        "qemu-system-aarch64",
        "-rtc", "base=localtime",
        "-M", "virt",
        "-cpu", "cortex-a57",
        "-m", "6M",
        "-bios", "limine-binary/BOOTAA64.EFI",
        "-drive", f"format=raw,file={img_file},if=none,id=drv0",
        "-device", "virtio-blk-device,drive=drv0",
        "-serial", "stdio"
    ]

    if debug_flags:
        qemu_cmd.extend(shlex.split(debug_flags))

else:
    print(
        "Unknown architecture. Supported: x86_64, aarch64"
    )
    sys.exit(1)

try:
    subprocess.run(qemu_cmd, check=False)
finally:
    if uefi_vars_copy and os.path.exists(uefi_vars_copy):
        os.remove(uefi_vars_copy)