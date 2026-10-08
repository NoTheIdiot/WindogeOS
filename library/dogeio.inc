%include "internal/syscalls.inc"

; Syscall arguments use rdi, rsi, rdx, then r10, matching the C wrappers.
%macro fs_read 3
    mov rax, READ_FILE
    mov rdi, %1
    mov rsi, %2
    mov rdx, %3
    syscall
%endmacro

%macro fs_write 2
    mov rax, WRITE_FILE
    mov rdi, %1
    mov rsi, %2
    syscall
%endmacro

%macro fs_append 3
    mov rax, APPEND_FILE
    mov rdi, %1
    mov rsi, %2
    mov rdx, %3
    syscall
%endmacro

%macro fs_create 1
    mov rax, CREATE_FILE
    mov rdi, %1
    syscall
%endmacro

%macro fs_delete 1
    mov rax, DELETE_FILE
    mov rdi, %1
    syscall
%endmacro

%macro fs_mkdir 1
    mov rax, CREATE_DIR
    mov rdi, %1
    syscall
%endmacro

%macro fs_rename 2
    mov rax, RENAME_FILE
    mov rdi, %1
    mov rsi, %2
    syscall
%endmacro

%macro fs_exists 1
    mov rax, FILE_EXISTS
    mov rdi, %1
    syscall
%endmacro

%macro fs_chdir 1
    mov rax, CHANGE_DIR
    mov rdi, %1
    syscall
%endmacro

%macro fs_get_cwd 2
    mov rax, GET_CWD
    mov rdi, %1
    mov rsi, %2
    syscall
%endmacro

%macro fs_delete_last_line 1
    mov rax, DELETE_LAST_LINE
    mov rdi, %1
    syscall
%endmacro

%macro fs_copy 2
    mov rax, COPY_FILE
    mov rdi, %1
    mov rsi, %2
    syscall
%endmacro

%macro io_print 1
    mov rax, PRINT
    mov rdi, %1
    syscall
%endmacro

%macro io_println 1
    mov rax, PRINTLN
    mov rdi, %1
    syscall
%endmacro

%macro io_clear 0
    mov rax, CLEAR
    syscall
%endmacro

%macro io_input 3
    mov rax, INPUT
    mov rdi, %1
    mov rsi, %2
    mov rdx, %3

    syscall
%endmacro

%macro io_get_key 0
    mov rax, GET_KEY
    syscall
%endmacro

%macro io_print_at 4
    mov rax, PRINT_AT
    mov rdi, %1
    mov rsi, %2
    mov rdx, %3
    mov r10, %4
    syscall
%endmacro

%macro io_text_color 1
    mov rax, TEXT_COLOR
    mov rdi, %1
    syscall
%endmacro

%macro io_background_color 1
    mov rax, BACKGROUND_COLOR
    mov rdi, %1
    syscall
%endmacro

%macro process_exit 1
    mov rax, EXIT
    mov rdi, %1
    syscall
%endmacro

%macro println 1
    io_println %1
%endmacro

%macro sys_exit 1
    process_exit %1
%endmacro