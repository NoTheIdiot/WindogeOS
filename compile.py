# import
import subprocess

# main
with open("compilefile.txt", "r") as file:
    n = 1
    gcc = "gcc -m32 -ffreestanding -nostdlib -Wall -Wextra -Werror -O2"
    for doge in file:
        doge = doge.strip()
        if doge.startswith("#") or not doge:
            continue

        elif doge.startswith("dogec"):
            shibe = doge.replace("dogec", gcc)

            nstring = str(n) + " "
            print(nstring + shibe)
            result = subprocess.run(shibe, shell=True)
            n += 1

            if result.returncode != 0:
                print("[Such Error] Compile Failed")
                break

        else:
            nstring = str(n) + " "
            print(nstring + doge)
            result = subprocess.run(doge, shell=True)
            n += 1

            if result.returncode != 0:
                print("[Such Error] Compile Failed")
                break
