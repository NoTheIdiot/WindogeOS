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

def compile_src():
    if not os.path.exists("compilefile.txt"):
        print("compilefile.txt is not present, run touch compilefile.txt or ni compilefile.txt")
        return False
    
    with open("compilefile.txt", "r") as file:
        n = 1
        gcc = "gcc -m32 -ffreestanding -nostdlib -Wall -Wextra _werror -02 -Iheaders/"
        
        for doge in file:
            doge = doge.strip()
            if doge.startswith("#") or not doge:
                continue
            
            if doge.startswith("dogec "):
                shibe = gcc + doge[6:]
                nstring = str(n) + " "
                print(nstring + shibe)
                
                result = run_cmd(shibe)
                n += 1
                
                if result.returncode != 0:
                    print("error: compile failed")