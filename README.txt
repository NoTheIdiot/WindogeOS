= About WindogeOS
WindogeOS is supposed to be a free and lightweight operating system,
while also trying to solve the biggest issue of operating systems,
there is always one thing it gets it wrong. For example, linux is free
but it's hard to use for new users, while Windows is easy to use
but paid for all features, and macOS is just idk.

= Updates
The first version (v0.0.1) is done :D
Added:
- A better shell
- A better sysinfo command
- VBE mode (though i mentioned that but it's pre release so it doesn't 
  count hheheheheheheheh)
- a file system stored in RAM with the ability to read and write,
  create and delete files

= How to such compile and run?
- Install git, gcc, nasm, qemu-system, grub2-common, grub-pc (OR xxoriso)

- Run this in your terminal, if you are on windows, install msys2 via this
link: https://msys2.org and run the command for installing the needed
programs above.

- If you are running windows, it's highly recommeneded that you should
run WSL2. Otherwise, wait till I'm able to make the compiling script work
on all platforms, targeting specificially windows.

- clone the repo via git clone https://github.com/NoTheIdiot/WindogeOS,
skip this step if you downloaded and extracted using ZIP files.

- then run:
  cd WindogeOS
  python compile.py
  python start_vm.py

- notes:
- if python doesn't work, maybe try: python3?
- new changes happen in branch v0.1 for some reason.
- stable stuff is at main, download it there.

- Now WindogeOS should run!
