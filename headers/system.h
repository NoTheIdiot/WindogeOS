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
extern char current_user[64];
extern uint32_t old;
extern char* dogeshell_version;

void system_editor(char* filename);
void system_settings();

uint64_t get_ram_end_address(void);
int system_create_user(char* name, char* password);
int system_verify_user(const char* name, char* password);

void setup_ring3_memory(uint64_t user_code_virt, uint64_t user_stack_virt, const uint8_t *user_code, size_t code_size);
void map_user_page(uint64_t virt_addr, uint64_t phys_addr);

// system calls
#define FS_READ         01
#define FS_WRITE        02

#define DOGEIO_PRINT    11
#define DOGEIO_CLEAR    12

#define EXIT            0

#endif