import subprocess

def cmd(command):
    subprocess.run(command, shell=True)

command = "qemu-system-x86_64 -drive file=windoge.img,format=raw,index=0,media=disk -m 16M -vga std"

cmd(command)
