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
- `dogescript <file> [arguments...]` runs a Dogescript script. See
  [Dogescript scripting](#dogescript-scripting) for its commands and syntax.
- `pages <file>` reads a text file one screen at a time. Use Up/Down or
  Space/`b` to navigate, `g` to return to the beginning, and `q` to quit.

## Dogescript scripting

Dogescript provides variables, input/output, arithmetic, file operations,
conditions, loops, and access to Dogeshell commands. Scripts are limited to
65535 bytes and 255 characters per line. Blank lines and lines whose first
non-space character is `#` are ignored. Quoted strings and backslash escapes
are supported.

```text
# Variables and arguments
set name "$1"
echo "Hello, $name"

# Arithmetic, comparisons, and branching
set count 1
math count $count + 1
if $count >= 2
    println "The count is at least two"
else
    println "The count is less than two"
endif

# Loops
repeat 3
    echo "Repeated"
endrepeat

while $count < 5
    inc count
endwhile

# File helpers
write /tmp/greeting.txt "Hello, $name"
read greeting /tmp/greeting.txt
println $greeting
```

Built-in commands are `echo`, `print`, `println`, `set`, `unset`, `input`,
`math`, `inc`, `dec`, `read`, `write`, `append`, `exists`, `isdir`, `assert`,
`fail`, and `exit`. `read` stores text files up to 255 bytes in a variable.
`exists <path> -> <variable>` and `isdir <path> -> <variable>` store `1` or `0`.
`math <variable> <integer> <+|-|*|/> <integer>` performs checked signed
64-bit integer arithmetic. Conditions use `==`, `!=`, `<`, `<=`, `>`, or `>=`;
two integer operands are compared numerically, otherwise they are compared as
text. `if` blocks use `else` and `endif`; loops use `while`/`endwhile` or
`repeat <count>`/`endrepeat`, with `break` and `continue`.

Variables can be expanded as `$name` or `${name}`. `$0` is the script path,
`$1` through `$N` are the arguments after the script path, and `$argc` is their
count. Other commands are passed to Dogeshell. Execution stops on the first
failed command unless the script uses `exit`; `assert` and `fail` also return
failure. A 500000-command execution limit prevents runaway scripts.

## Contributing
Thanks for contributing for some reason...
You can do anything but remind me if there are alot of or major 
changes in your fork, thanks :D
