import subprocess
import os

def cmd(command):
    subprocess.run(command, shell=True)

command = "qemu-system-x86_64 -kernel windoge.bin -m 16M"

cmd(command)