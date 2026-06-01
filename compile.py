import subprocess
import shutil
import os
import platform

system = platform.system()

def cmd(command):
    return subprocess.run(command, shell=True, capture_output=True)

def compile_all_files():
    if not os.path.exists("compilefile.txt"):
        print("[Not Wow] compilefile.txt is missing!")
        return False

    with open("compilefile.txt", "r") as file:
        n = 1
        gcc = "gcc -m32 -ffreestanding -nostdlib -Wall -Wextra -Werror -O2 -Iheaders/ "
        
        for doge in file:
            doge = doge.strip()
            if doge.startswith("#") or not doge:
                continue


            if doge.startswith("dogec "):
                shibe = gcc + doge[6:]
                nstring = str(n) + " "
                print(nstring + shibe)
                
                result = subprocess.run(shibe, shell=True)
                n += 1

                if result.returncode != 0:
                    print("[Such Error] your code sucks")
                    return False

            elif doge == "linklinkering":
                print("linking files, pray that it works")
                if system == "Windows":
                    result = subprocess.run(["tools\\link.bat"], shell=True, capture_output=True, text=True)
                    print(result.stdout)
                else:
                    result = subprocess.run("bash tools/link.sh", shell=True, capture_output=True, text=True)
                    print(result.stdout)

            elif doge == "clean":
                print("cleaning object files")
                if system == "Windows":
                    result = subprocess.run(["tools\\clean.bat"], shell=True, capture_output=True, text=True)
                    print(result.stdout)
                else:
                    result = subprocess.run("bash tools/clean.sh", shell=True, capture_output=True, text=True)
                    print(result.stdout)

            else:
                nstring = str(n) + " "
                print(nstring + doge)
                
                result = subprocess.run(doge, shell=True)
                n += 1

                if result.returncode != 0:
                    print("[Such Error] compile failed")
                    return False
    return True

def create_grub_iso():
    if not os.path.exists("windoge.bin"):
        print("[Not Wow] windoge.bin missing, cannot bake image.")
        return False

    if os.path.exists("isoroot"):
        shutil.rmtree("isoroot")
    
    os.makedirs("isoroot/boot/grub", exist_ok=True)
    shutil.copy("windoge.bin", "isoroot/windoge.bin")
    shutil.copy("windoge.bin", "isoroot/boot/windoge.bin")
    
    if os.path.exists("grub.cfg"):
        shutil.copy("grub.cfg", "isoroot/boot/grub/grub.cfg")
    else:
        print("[Warning] No grub.cfg found, GRUB will drop to rescue prompt.")

    iso_created = False

    # Try GRUB tool native runtime
    result = cmd("which grub-mkrescue")
    if result.returncode == 0:
        print("[Such Notes] Using GRUB to make the ISO...")
        subprocess.run("grub-mkrescue -o windoge.iso isoroot 2>/dev/null", shell=True)
        iso_created = os.path.exists("windoge.iso") and os.path.getsize("windoge.iso") > 0

    # Fallback to absolute bare toolchains if eltorito modules exist locally
    if not iso_created:
        result = cmd("which xorriso")
        if result.returncode == 0:
            print("[Such Notes] Using xorriso to make the ISO...")
            subprocess.run("xorriso -as mkisofs -R -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table -o windoge.iso isoroot 2>/dev/null", shell=True)
            iso_created = os.path.exists("windoge.iso") and os.path.getsize("windoge.iso") > 0
    
    if not iso_created:
        result = cmd("which mkisofs")
        if result.returncode == 0:
            print("[Such Notes] Using mkisofs to create ISO...")
            subprocess.run("mkisofs -R -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table -o windoge.iso isoroot 2>/dev/null", shell=True)
            iso_created = os.path.exists("windoge.iso") and os.path.getsize("windoge.iso") > 0
    
    return iso_created

# Main execution tree flow
if compile_all_files():
    print("[Such Notes] making such ISO...")
    if create_grub_iso():
        print("[Wow] Such wow, compile success.")
    else:
        print("[Not Wow] Could not create GRUB ISO. Trying direct kernel load...")
        print("[Not Wow] Note: Graphics mode (VBE) requires GRUB bootloader")
        print("[Such Notes] Booting in text mode fallback...")
else:
    print("[Not Wow] Compilation sequence stopped. ISO skipped.")
