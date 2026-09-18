""" This is the build config script
    if you git cloned this repo, then configure this
    script for your own os project. Shit ass garbage, 
    but it works.
"""

# metadata
project_name = "WindogeOS"
img_file     = "windoge_os.img"
default_arch = "x86_64"

# tools of your choice, or it's for the full path
tools = {
    "c_compiler" : "clang",
    "assembler"  : "nasm",
    "linker"     : "ld.lld",
    "objcopy"    : "llvm-objcopy",
}

# compiler flags for each arch of your choice
# or you can put more
compiler_flags = {
    "x86_64" : {
        "kernel_flags"        : "-target x86_64-unknown-none-elf -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel",
        "app_flags"           : "-target x86_64-unknown-none-elf -m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=large -DWINDOGE_APP",
        "kernel_linker_script": "linker-files/x86_64.ld",
        "app_linker_script"   : "linker-files/apps.ld",
        "linker_flags"        : "-m elf_x86_64",
        "limine_sys"          : "binaries/limine-bios.sys",
    },

    "arm64" : {
        "kernel_flags"        : "-target aarch64-unknown-none-elf -mcpu=generic -march=armv8-a -mgeneral-regs-only",
        "app_flags"           : "-target aarch64-unknown-none-elf -mcpu=generic -march=armv8-a",
        "kernel_linker_script": "linker-files/arm64.ld",
        "app_linker_script"   : "linker-files/apps.ld",
        "linker_flags"        : "-m aarch64elf",
        "limine_sys"          : "binaries/limine-uefi-aarch64.efi",
    }
}

# common compiler flags that will be used for all args
common_flags = (
    "-Wall -Wextra -Werror -Wconversion -std=gnu11 -nostdinc -ffreestanding "
    "-fno-stack-protector -fno-stack-check -fno-lto -fno-PIC -fno-pie "
    "-ffunction-sections -fdata-sections -Iheaders"
)

# source folders, where your files compile from
# and the apps source folder where the apps are compiled from
source_folders_regular = ["kernel", "helper"]
source_folders_apps    = ["apps"]