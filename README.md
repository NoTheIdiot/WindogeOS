## About WindogeOS
not unix

* Doge filled OS that fits in just 4 floppies
* Free and lightweight operating system
* Easy to use on the command line
* Doesn't make your RAM want to commit sudoku

## Minimum requirements
Most x86_64 CPUs will work. Intel Atoms, early Pentiums (2000-2010), and Core 2 Duos are well tested
and very compatible.

Literally just 2MB of SSD and 2MB of RAM

## Updates
To see updates, read commits and release descriptions.

## How to compile and run?
__*Note*__: these instructions are meant for Debian / Ubuntu.
Building on Arch Linux and other distros is possible by changing a few packages/package names.

### Compiling
If you are on Windows, install WSL with `wsl --install Ubuntu`.

- install `git`, `python`, `clang`, `lld`, `llvm`, `mtools`, `exfatprogs`,
  and `exfat-fuse`. Install `ovmf` to run a UEFI image in QEMU.

- clone the github repo via `git clone https://github.com/NoTheIdiot/WindogeOS`

- cd into project directory (`cd WindogeOS`)

- compile source code: `python compile.py`
  
  that's it, you might have to type in your sudo password
  for `compile.py`

### Booting on real hardware (very wow)
__*Note*__: It's probably not a good idea to flash WindogeOS to your main machine. A test 
machine or an older spare machine is a better idea.

WindogeOS can now support SATA, but not USB, even just a USB keyboard 
driver. Nothing special, do the same flashing.

To flash to a hard drive:
`sudo dd if=/path/to/WindogeOS/windoge_os.img of=your_drive bs=4M status=progress conv=fsync`


## Contributing
Thanks for contributing :) Every PR helps end the RAM crisis :D

You can do anything but remind me if there are alot of or major 
changes in your fork, thanks :D
