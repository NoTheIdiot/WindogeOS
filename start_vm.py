import subprocess
import os
import shutil

def cmd(command):
    subprocess.run(command, shell=True, check=False)

def create_grub_iso():
    if os.path.exists("isoroot"):
        shutil.rmtree("isoroot")
    
    os.makedirs("isoroot/boot/grub", exist_ok=True)
    shutil.copy("windoge.bin", "isoroot/windoge.bin")
    shutil.copy("windoge.bin", "isoroot/boot/windoge.bin")
    shutil.copy("grub.cfg", "isoroot/boot/grub/grub.cfg")
    iso_created = False

    result = subprocess.run("which grub-mkrescue", shell=True, capture_output=True)
    if result.returncode == 0:
        print("[Such Notes] Using Grub to make the ISO")
        cmd("grub-mkrescue -o windoge.iso isoroot 2>/dev/null")
        iso_created = os.path.exists("windoge.iso") and os.path.getsize("windoge.iso") > 0

    if not iso_created:
        result = subprocess.run("which xorriso", shell=True, capture_output=True)
        if result.returncode == 0:
            print("[Such notes] Using xorriso to make the ISO")
            cmd("xorriso -as mkisofs -R -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table -o windoge.iso isoroot 2>/dev/null")
            iso_created = os.path.exists("windoge.iso") and os.path.getsize("windoge.iso") > 0
    
    if not iso_created:
        result = subprocess.run("which mkisofs", shell=True, capture_output=True)
        if result.returncode == 0:
            print("[*] Using mkisofs to create ISO...")
            cmd("mkisofs -R -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table -o windoge.iso isoroot 2>/dev/null")
            iso_created = os.path.exists("windoge.iso") and os.path.getsize("windoge.iso") > 0
    
    return iso_created

print("making such ISO...")
if create_grub_iso():
    print("booting...")
    command = "qemu-system-x86_64 -cdrom windoge.iso -m 256M -vga std"
    cmd(command)
else:
    print("[Not Wow] Could not create GRUB ISO. Trying direct kernel load...")
    print("[Not Wow] Note: Graphics mode (VBE) requires GRUB bootloader")
    print("[Note] Booting in text mode fallback...")
    command = "qemu-system-x86_64 -kernel windoge.bin -m 256M"
    cmd(command)