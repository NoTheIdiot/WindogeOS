## About WindogeOS
* Doge filled OS that fits in just 4 floppies
* Free and lightweight operating system
* Easy to use on the command line
* Doesn't make your RAM want to commit sudoku

## Minimum requirements
Most x86_64 CPUs will work. Intel Atoms, early Pentiums (2000-2010), and Core 2 Duos are well tested
and very compatible.

Your PC must be able to run WindogeOS in BIOS mode. UEFI PCs in CSM mode will function too.
You will also need 6MB of RAM and 8MB of storage.

## Recommended Specs
Any CPU, 8MB of RAM and 12MB of storage for the best experience.
Dogescript for advanced scripts also needs 10MB of RAM.

## Updates
To see updates, read commits and release descriptions.

## How to compile and run?
__*Note*__: these instructions are meant for Debian / Ubuntu.
Building on Arch Linux and other distros is possible by changing a few packages/package names.

### Compiling
If you are on Windows, install WSL with `wsl --install Ubuntu`.

- install `git`, `python`, `clang`, `lld`, `llvm`, `mtools`, `exfatprogs`, and `exfat-fuse`

- clone the github repo via `git clone https://github.com/NoTheIdiot/WindogeOS`

- cd into project directory (`cd WindogeOS`)

- compile source code: `python compile.py`
  
  that's it, you might have to type in your sudo password
  for `compile.py`

Applications are built and installed as ELF `.elf` files. Packages retain their
compressed `.rle` archive format and install as ELF applications.

### Synchronous process stacking
User applications can run another ELF application without discarding their
current address space by calling `process_stack` from `library/dogelib.h`:

```c
char *arguments[] = {"--example"};
uint64_t status = process_stack("apps/example.elf", 1, arguments);
```

The call waits for the child to exit, then resumes the caller and returns the
child's exit status. It returns `(uint64_t)-1` if the child cannot be started.
At most four stacked child processes are allowed at once.

Oh yeah, it probably sucks for now.

### Running in VM
Run `python start_vm.py`. This will open the .img file in QEMU.
If you would like to run WindogeOS in a different VM for some reason, open the .img file in your preferred VM.

### Booting on real hardware (very wow)
__*Note*__: It's probably not a good idea to flash WindogeOS to your main machine. A test 
machine or an older spare machine is a better idea.

To boot WindogeOS on real hardware, you have to flash to a medium that WindogeOS can write to.
USB drives are not supported storage mediums, only IDE devices at the moment.

To flash to a hard drive:
`sudo dd if=/path/to/WindogeOS/windoge_os.img of=your_drive bs=4M status=progress conv=fsync`

SD card to IDE adapters might work, this is untested though.

## Contributing
Thanks for contributing :) Every PR helps end the RAM crisis :D

You can do anything but remind me if there are alot of or major 
changes in your fork, thanks :D
