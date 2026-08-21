#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <bool.h>
#include <stddef.h>

int system_dogeshell_ex(char* command);
void system_dogeshell();
void system_bash();

void system_fetch();
extern char* windoge_version;
extern uint32_t old;

void system_editor(char* filename);
void system_settings();

#endif