#ifndef SYSCALLS_H
#define SYSCALLS_H

// process management
#define Process_Exit 0

// file system operations
#define WriteFile   11
#define ReadFile    12
#define CreateFile  13
#define CreateDir   14
#define DeleteThing 15
#define Exists      16

// user input and output
#define Print 20
#define Input 21

#endif