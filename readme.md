# About WindogeOS
Doge filled OS that fits in a (3) floppies, probably useless.
Also supposed to be a free and lightweight operating system.
Also being easy to use on the command line.

## Minimum requirements
Any x86_64 CPU will work, and also you will ned at least
4MB of RAM and 4MB of disk, and it will run pretty well.

## Recommended Specs
Any CPU, 6MB of RAM and 8MB of disk for the best experience.

## Updates
Nothing special read commits and releases

## How to such compile and run?
- Install the following
    git
    python
    clang
    lld
    llvm
    mtools
    exfatprogs
    exfat-fuse

- Run this in your terminal, if you are on windows, install wsl by typing
  "wsl --install Ubuntu"
- clone the repo via [git clone https://github.com/NoTheIdiot/WindogeOS]

- then run:
  cd WindogeOS
  python compile.py
  python start_vm.py

  that's it, you might have to type in your sudo password
  for compile.py

## Applications
- `file_manager [directory]` interactively lists and manages files and directories.
  Use `help` inside the app to see its commands.
- `hexdump <file>` prints a file as hexadecimal bytes and readable ASCII.
- `pages <file>` reads a text file one screen at a time. Use Up/Down or
  Space/`b` to navigate, `g` to return to the beginning, and `q` to quit.

## Contributing
Thanks for contributing for some reason...
You can do anything but remind me if there are alot of or major 
changes in your fork, thanks :D
