# About WindogeOS
Doge filled OS that fits in a (3) floppies, probably useless.
Also supposed to be a free and lightweight operating system.
Also being easy to use on the command line.

## Minimum requirements
Pretty much any x86_64 will work, intel atoms, core 2 duos (if specified 
x86_64), anything would work. You will also need 4MB of RAM and 5MB of 
disk.

## Recommended Specs
Any CPU, 6MB of RAM and 8MB of disk for the best experience.
Dogescript for advanced scripts also needs 8MB of RAM.

## Updates
Nothing special read commits and releases

## Installation, how to much compile and run?
*__Note__: these instructions are meant for Debian / Ubuntu.
Building on Arch Linux and other distros is possible by changing a few packages and a symlink or two.
*__Also__: these instructions for starting the OS in a VM, if you
want to run this on real hardware, skip `python3 start_vm.py`

### What's needed
If you are on Windows, install WSL with `wsl --install Ubuntu`.

- Install the following:
    `git` 
    `python` 
    `clang` 
    `lld` 
    `llvm` 
    `mtools` 
    `exfatprogs` 
    `exfat-fuse`

- clone the github repo via `git clone https://github.com/NoTheIdiot/WindogeOS`

- then run:
  ```
  cd WindogeOS
  python compile.py
  python start_vm.py
  ```

  that's it, you might have to type in your sudo password
  for compile.py

### Installation (to real hardware very wow)
*__note__: It's probably not a good idea to install this on your main machine. A test 
machines or an older spare machine is a better idea.
To install on real hardware, you have to flash to a medium that WindogeOS can write to.
USB drives are not supported storage mediums right now, only IDE devices.

If you are going to flash it to a hard drive:
`sudo dd if=/path/to/WindogeOS/windoge_os.img of=your_drive bs=4M status=progress conv=fsync`

You may use SD card to IDE adapters,
so in that case that might somehow work.

## Contributing
Thanks for contributing :)
You can do anything but remind me if there are alot of or major 
changes in your fork, thanks :D
