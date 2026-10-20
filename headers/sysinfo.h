#ifndef SYSINFO_H
#define SYSINFO_H

#include <system.h>

// i freaking hate that the compiler said const char* is not 
// compatible with char* even if they are the same
char *windoge_version = "WindogeOS v0.2.1";
extern const char *windoge_arch;

#endif