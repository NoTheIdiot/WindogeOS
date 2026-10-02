= About WindogeOS
Doge filled OS that fits in a floppy, probably useless.
Also supposed to be a free and lightweight operating system.
Also being easy to use on the command line.

= The hell's inside?
This piece of probably useless software has the following
- exFAT file system
- basic text editor
- RTC time
- basic syscalls that blows up with args
- uses limine 64 bit so it's not slow as fu
- full idt, gdt and tss
- shell that actually makes since

= Updates
Nothing special read commits and releases

= How to such compile and run?
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

= Contributing
  Thanks for contributing for some reason...
  You can do anything but remind me if there are alot of or major 
  changes in your fork, thanks :D
