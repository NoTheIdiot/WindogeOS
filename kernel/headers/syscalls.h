#ifndef SYSCALLS_H
#define SYSCALLS_H

// process management
#define Process_Exit 0

// file system operations
#define WriteFile        11
#define ReadFile         12
#define CreateFile       13
#define CreateDir        14
#define DeleteThing      15
#define Exists           16
#define ListDirectory    17
#define ChangeDirectory  18
#define CurrentDirectory 19

// user input and output
#define Print 20
#define Input 21

// system operations
#define Shutdown    30
#define Reboot      31
#define Username    32
#define Uname       33

// other things that might be used
#define Beep    50

#endif