= About WindogeOS
WindogeOS is supposed to be a free and lightweight operating system,
while also trying to solve the biggest issue of operating systems,
there is always one thing it gets it wrong. For example, linux is free
but it's hard to use for new users, while Windows is easy to use
but paid for all features, and macOS is just idk.

= Updates
I've made a FAT32 file system, though VBE no longer works, if you can,
pls send me a solution.

= How to such compile and run?
- Install git, gcc, nasm, and qemu-system, grub-common2

- Run this in your terminal, if you are on windows, install msys2 via this
link: https://msys2.org and run the command for installing the needed
programs above.

- clone the repo via git clone https://github.com/NoTheIdiot/WindogeOS

- then run:
  cd WindogeOS
  python compile.py
  python start_vm.py

- Now WindogeOS should run!

= Notes
- if the python command doesn't work, try using python3.
- do not make the qemu-system command yourself, run python start_vm.py.
  if you somehow got a better result, please make a pull request,
  less headaches are better.


= Notes for development
- the infiles folder at root is for files going to be inject to FAT32.
- the dump folder is used for deleted code that could be reused in the future
  for a feature that is not needed before.