import subprocess
import shutil
import os
import platform

system = platform.system()

if system == "Windows":
    linker_var = "clang --target=i386-pc-none-elf -ffreestanding -nostdlib -fuse-ld=lld -Wl, -T, linker/linker.ld"
else:
    linker_var = "ld -m elf-i386 -T linker/linker.ld"

def run_cmd(command):
    return subprocess.run(command, shell=True)

def check_mtools():
    try:
        shutil.which("mformat")
        return True
    except Exception:
        return False

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
    if not check_mtools():
        print("[!] FATAL ERROR: 'mtools' package is missing on your host machine!")
        print("    -> Linux: Run 'sudo apt install mtools'")
        print("    -> macOS: Run 'brew install mtools'")
        print("    -> Windows: Install mtools binaries or verify your environment PATH variable.")
        return False

    if not os.path.exists("windoge.bin"):
        print("windoge.bin missing, cannot bake image.")
        return False

    final_img = "windoge.img"
    
    if os.path.exists(final_img):
        os.remove(final_img)
        
    print("[*] Allocating clean 32MB disk matrix storage...")
    img_size_bytes = 32 * 1024 * 1024
    with open(final_img, "wb") as f:
        f.seek(img_size_bytes - 1)
        f.write(b"\0")

    print("[*] Formatting drive space as flat authentic FAT32...")
    mformat_cmd = f"mformat -F -c 1 -m 0xf8 -h 16 -s 32 -t 128 -v WINDOGE -i {final_img} ::"
    run_cmd(mformat_cmd)

    print("[*] Structuring internal system path maps...")
    run_cmd(f"mmd -i {final_img} ::boot")

    print("[*] Injecting windoge.bin kernel structure...")
    run_cmd(f"mcopy -o -i {final_img} windoge.bin ::boot/windoge.bin")

    asset_source_dir = "infiles"
    if not os.path.exists(asset_source_dir):
        os.makedirs(asset_source_dir)
        
    target_readme_file = os.path.join(asset_source_dir, "readme.txt")
    target_shiba_file = os.path.join(asset_source_dir, "random.txt")
    
    if not os.path.exists(target_readme_file) or os.path.getsize(target_readme_file) == 0:
        with open(target_readme_file, "w") as f:
            f.write("Welcome to WindogeOS!\n")
            f.write("This operating system now finally has FAT32 after...7 attempts.\n")
            f.write("Well, it's read only for now.\n")
            f.write("Enjoy\n")

    with open(target_shiba_file, "w") as f:
        f.write("#    #  ####  #    # \n")
        f.write("#    # #    # #    # \n")
        f.write("#    # #    # #    # \n")
        f.write("# ## # #    # # ## # \n")
        f.write("##  ## #    # ##  ## \n")
        f.write("#    #  ####  #    # \n")

    print(f"[*] Copying user assets from '{asset_source_dir}' into root FAT cluster maps...")
    for root, dirs, files in os.walk(asset_source_dir):
        for file in files:
            local_file_path = os.path.join(root, file)
            target_fat_name = file.upper()
            run_cmd(f"mcopy -o -i {final_img} {local_file_path} ::{target_fat_name}")
            print(f"    -> Injected file: {target_fat_name}")

    return True

if compile_src():
    print("creating FAT32 disk image...")
    if create_fat32_image():
        print("compile success")
    else:
        print("build failed")
else:
    print("are you debugging mr squidward")
    clean_object_files()
