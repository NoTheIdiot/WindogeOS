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
extern char computer_name[64];

void system_editor(char* filename);
void system_settings();

uint64_t get_ram_end_address(void);
int system_create_user(char* name, char* password, int permission_id);
int system_verify_user(const char* name, char* password);
int system_can_access_path(const char* username, const char* target_path);
void system_run_exec(char *filename, int program_size);

void setup_ring3_memory(uint64_t user_code_virt, uint64_t user_stack_virt, const uint8_t *user_code, size_t code_size);
void map_user_page(uint64_t virt_addr, uint64_t phys_addr);
uint64_t pmm_alloc_zeroed_page(void);
extern void to_userland_ring3(uint64_t user_rip, uint64_t user_rsp) __attribute__((noreturn));

// system calls
#define FS_READ	01
#define FS_WRITE 02
#define FS_EXISTS 03
#define FS_CREATE 04
#define FS_MKDIR 05
#define FS_DELETE 06 
#define FS_RENAME 07
#define FS_READ_RAW 08

#define DOGEIO_PRINT 11
#define DOGEIO_CLEAR 12
#define DOGEIO_INPUT 13
#define DOGEIO_COLOR 14
#define DOGEIO_BACKGROUND 15
#define DOGEIO_CLEAR_RAW 16

#define SYS_EXIT 0

#endif
