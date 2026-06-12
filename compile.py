# import modules
import subprocess
import shutil
import os
import platform

# used because you can't just do
# if platform.system == "Windows":
system = platform.system()

# used so I can try to target cross platform
if system == "Windows":
    linker_var = "ld.lld"
else:
    linker_var = "ld"

# to run commands easier
def run_cmd(command):
    return subprocess.run(command, shell=True, capture_output=True)

# clean up object files
def clean_object_files():
    if system == "Windows":
        run_cmd("./tools/clean.bat")
    else:
        run_cmd("sh tools/clean.sh")

def compile_src():
    if not os.path.exists("compilefile.txt"):
        print("compilefile.txt is not present, run touch compilefile.txt or ni compilefile.txt")
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
                    print("error: compile failed")
                    return False
                    
            elif doge.startswith("dogelink"):
                shibe = doge.replace("dogelink", "linker_var")
                nstring = str(n) + " "
                print(nstring)
                
                result = subprocess.run(shibe, shell=True)
                n += 1
                
                if result.returncode != 0:
                    print("error: compile failed.")
                    return False
                
            else:
                nstring = str(n) + " "
                print(nstring + doge)
                
                result = subprocess.run(doge, shell=True)
                n += 1

                if result.returncode != 0:
                    print("error: compile failed")
                    return False
    return True

def create_grub_iso():
    if not os.path.exists("windoge.bin"):
        print("windoge.bin missing, cannot bake image.")
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
        print("using GRUB to make the ISO...")
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
            print("using mkisofs to create ISO...")
            subprocess.run("mkisofs -R -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table -o windoge.iso isoroot 2>/dev/null", shell=True)
            iso_created = os.path.exists("windoge.iso") and os.path.getsize("windoge.iso") > 0
    
    return iso_created

# execute 
if compile_src():
    print("creating iso...")
    if create_grub_iso():
        print("compile success, doesn't mean that it will boot >:)")
    else:
        print("can't create grub iso, direct kernel load.")
else:
    print("are you debugging mr squidward")
    clean_object_files()