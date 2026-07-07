= About WindogeOS
WindogeOS is supposed to be a free and lightweight operating system,
while also trying to solve the biggest issue of operating systems,
there is always one thing it gets it wrong. For example, linux is free
but it's hard to use for new users, while Windows is easy to use
but paid for all features, and macOS is just idk.

= Updates
Currently converting to a RAMFS, if you got a solution for a REAL file system,
please create a pull request.

= How to such compile and run?
- Install git, clang, and qemu-system. Yes, the other tools (like limine, that's it)
  is already included.

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
- the files folder will be injected to the filesystem.
- the help folder is just a reference for me.
