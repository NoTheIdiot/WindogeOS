#ifndef SYSINFO_H
#define SYSINFO_H

#include <system.h>

// i freaking hate that the compiler said const char* is not 
// compatible with char* even if they are the same
extern char *windoge_version;
extern const char *windoge_arch;
extern const char* doge_ascii[22];
extern const char* doge_ascii_full[24];

#endif