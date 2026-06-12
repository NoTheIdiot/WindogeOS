import subprocess
import shutil
import os
import platform

system = platform.system()

if system == "Windows":
    linker_var = "ld.lld"
else:
    linker_var = "ld"

def run_cmd(command):
    return subprocess.run(command, shell=True)

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
                shibe = doge.replace("dogelink", linker_var)
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

def create_fat32_image():
    if not os.path.exists("windoge.bin"):
        print("windoge.bin missing, cannot bake image.")
        return False

    final_img = "windoge.img"
    build_root = "fat_root"
    
    if os.path.exists(build_root):
        shutil.rmtree(build_root)
    if os.path.exists(final_img):
        os.remove(final_img)
        
    os.makedirs(f"{build_root}/boot/grub", exist_ok=True)
    shutil.copy("windoge.bin", f"{build_root}/boot/windoge.bin")
    
    with open(f"{build_root}/boot/grub/grub.cfg", "w") as cfg:
        cfg.write("set timeout=0\n")
        cfg.write("set default=0\n")
        cfg.write('menuentry "WinDoge OS" {\n')
        cfg.write("  multiboot /boot/windoge.bin\n")
        cfg.write("  boot\n")
        cfg.write("}\n")

    print("Generating bootable disk layout using GRUB utilities...")
    # grub-mkrescue natively bakes an image containing both ISO and flat MBR/FAT hard disk partitions
    run_cmd(f"grub-mkrescue -o {final_img} {build_root}")
    
    shutil.rmtree(build_root)
    
    # Pad or truncate the file out to your desired 128MB size bounds safely
    if os.path.exists(final_img):
        img_size_bytes = 128 * 1024 * 1024
        with open(final_img, "r+b") as f:
            f.truncate(img_size_bytes)
        return True
        
    return False

if compile_src():
    print("creating FAT32 disk image...")
    if create_fat32_image():
        print("compile success! Image baked cleanly.")
    else:
        print("failed to build structured FAT32 target disk layout.")
else:
    print("are you debugging mr squidward")
    clean_object_files()
