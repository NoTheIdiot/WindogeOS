import subprocess

def cmd(command):
    subprocess.run(command, shell=True)

command = "qemu-system-x86_64 -cdrom windoge.iso -m 16M -vga std"
cmd(command)