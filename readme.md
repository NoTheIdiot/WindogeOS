# About WindogeOS
Doge filled OS that fits in a (3) floppies, probably useless.
Also supposed to be a free and lightweight operating system.
Also being easy to use on the command line.

## Minimum requirements
Any x86_64 CPU will work, and also you will ned at least
4MB of RAM and 4MB of disk, and it will run pretty well.
Dogescript full functionality requires about 6MB of RAM.

## Recommended Specs
Any CPU, 6MB of RAM and 8MB of disk for the best experience.
Dogescript for advanced scripts also needs 8MB of RAM.

## Updates
Nothing special read commits and releases

## How to compile and run?
### In your terminal:
*__Note__: these instructions are meant for Debian / Ubuntu.
Building on Arch Linux and other distros is possible by changing a few packages 
and a symlink or two.
If you are on Windows, install WSL with* `wsl --install Ubuntu`.

- Install the following:
    `git` 
    `python` 
    `clang` 
    `lld` 
    `llvm` 
    `mtools` 
    `exfatprogs` 
    `exfat-fuse`

- clone the repo via `git clone https://github.com/NoTheIdiot/WindogeOS`

- then run:
  ```
  cd WindogeOS
  python compile.py
  python start_vm.py
  ```

  that's it, you might have to type in your sudo password
  for compile.py

## Contributing
Thanks for contributing :)
You can do anything but remind me if there are alot of or major 
changes in your fork, thanks :D
