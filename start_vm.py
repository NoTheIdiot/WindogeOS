import subprocess

def cmd(command):
    subprocess.run(command, shell=True)

command = "qemu-system-i386 -kernel windoge.bin -drive format=raw,file=windoge.img -vga std"

cmd(command)
