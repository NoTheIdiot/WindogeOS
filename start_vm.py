import subprocess

def cmd(command):
    subprocess.run(command, shell=True)

command = "qemu-system-i386 -kernel windoge.bin -drive file=windoge.img,format=raw,index=0,media=disk -vga std"

cmd(command)
