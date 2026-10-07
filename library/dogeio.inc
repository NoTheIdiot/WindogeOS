%define READ_FILE 1
%define WRITE_FILE 2
%define CREATE_FILE 3
%define DELETE_FILE 4
%define CREATE_DIR 5
%define RENAME_FILE 6
%define FILE_EXISTS 7
%define CHANGE_DIR 8
%define DELETE_LAST_LINE 9
%define COPY_FILE 10

%define PRINT 11
%define PRINTLN 12
%define CLEAR 13
%define INPUT 14
%define GET_KEY 15
%define PRINT_AT 16
%define TEXT_COLOR 17
%define BACKGROUND_COLOR 18

%define EXIT 60

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