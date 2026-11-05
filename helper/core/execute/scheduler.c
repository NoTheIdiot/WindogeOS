#include <boot/scheduler.h>
#include <boot/kernel.h>
#include <boot/syscall_internal.h>
#include <basicutil.h>
#include <dogeio.h>
#include <start.h>
#include <system.h>
#include <string.h>

#define TASK_KERNEL_STACK_SIZE 16384
#define MSR_IA32_GS_BASE 0xC0000101
#define MSR_IA32_KERNEL_GS_BASE 0xC0000102

typedef struct {
    uint32_t pid;
    uint32_t active;
    uint32_t workspace_id;
    uint64_t cr3;
    uint64_t gs_base;
    uint64_t kernel_gs_base;
    bool linux_abi;
    bool fs_context_valid;
    uint8_t drive;
    char cwd[260];
    interrupt_frame_t *frame;
    linux_task_context_t linux_context;
    program_memory_t program;
    uint8_t fpu_state[512] __attribute__((aligned(16)));
    uint8_t kernel_stack[TASK_KERNEL_STACK_SIZE] __attribute__((aligned(16)));
} scheduler_task_t;

static scheduler_task_t tasks[SCHEDULER_TASK_LIMIT];
static size_t current_task;
static uint32_t next_pid = 2;
static uint64_t kernel_cr3;
static uint64_t initial_kernel_stack_top;
uint8_t isr_fpu_temp[512] __attribute__((aligned(16)));

static uint64_t scheduler_disable_interrupts(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; pop %0; cli"
                      : "=r"(flags) : : "memory");
    return flags;
}

static void scheduler_restore_interrupts(uint64_t flags) {
    if ((flags & (1ULL << 9)) != 0) {
        sti();
    }
}

static void scheduler_save_fs_context(scheduler_task_t *task) {
    char *cwd = fs_dirname();
    if (cwd == NULL || str_strlen(cwd) >= sizeof(task->cwd)) {
        task->fs_context_valid = false;
        return;
    }
    memcpy(task->cwd, cwd, str_strlen(cwd) + 1);
    task->drive = fs_current_drive();
    task->fs_context_valid = true;
}

static void scheduler_restore_fs_context(const scheduler_task_t *task) {
    if (!task->fs_context_valid) {
        return;
    }
    if (fs_select_drive(task->drive) != 0 ||
        fs_chdir((char *)task->cwd) != 0) {
        log("[scheduler] failed to restore task working directory");
    }
}

static void scheduler_activate(size_t index) {
    scheduler_task_t *task = &tasks[index];
    current_task = index;
    switch_user_pml4(task->cr3);
    gdt_set_kernel_stack(index == 0 ? initial_kernel_stack_top :
                         (uint64_t)&task->kernel_stack[TASK_KERNEL_STACK_SIZE]);
    scheduler_restore_fs_context(task);
    syscall_select_linux_abi(task->linux_abi);
    if (task->linux_abi) {
        linux_syscall_context_restore(&task->linux_context);
    }
    wrmsr(MSR_IA32_GS_BASE, task->gs_base);
    wrmsr(MSR_IA32_KERNEL_GS_BASE, task->kernel_gs_base);
    __asm__ volatile ("fxrstor64 %0" : : "m"(task->fpu_state));
    sys_workspace_load(task->workspace_id,
                       task->workspace_id == (uint32_t)current_terminal_id);
}

static size_t scheduler_find_next(size_t after) {
    for (size_t step = 1; step <= SCHEDULER_TASK_LIMIT; step++) {
        size_t index = (after + step) % SCHEDULER_TASK_LIMIT;
        if (tasks[index].active) {
            return index;
        }
    }
    return 0;
}

void scheduler_init(void) {
    memset(tasks, 0, sizeof(tasks));
    current_task = 0;
    kernel_cr3 = read_cr3() & ~0xFFFULL;
    initial_kernel_stack_top = gdt_kernel_stack_top();
    tasks[0].active = 1;
    tasks[0].cr3 = kernel_cr3;
    tasks[0].pid = 0;
    tasks[0].gs_base = rdmsr(MSR_IA32_GS_BASE);
    tasks[0].kernel_gs_base = rdmsr(MSR_IA32_KERNEL_GS_BASE);
    scheduler_save_fs_context(&tasks[0]);
    linux_syscall_context_init(&tasks[0].linux_context);
    __asm__ volatile ("fxsave64 %0" : "=m"(tasks[0].fpu_state));
    memcpy(isr_fpu_temp, tasks[0].fpu_state, sizeof(isr_fpu_temp));
}

interrupt_frame_t *scheduler_on_timer(interrupt_frame_t *frame) {
    if (frame == NULL || !tasks[current_task].active) {
        return frame;
    }

    scheduler_task_t *outgoing = &tasks[current_task];
    outgoing->frame = frame;
    outgoing->cr3 = read_cr3() & ~0xFFFULL;
    outgoing->gs_base = rdmsr(MSR_IA32_GS_BASE);
    outgoing->kernel_gs_base = rdmsr(MSR_IA32_KERNEL_GS_BASE);
    outgoing->linux_abi = syscall_linux_abi_active();
    if (outgoing->linux_abi) {
        linux_syscall_context_save(&outgoing->linux_context);
    }
    scheduler_save_fs_context(outgoing);
    memcpy(outgoing->fpu_state, isr_fpu_temp, sizeof(isr_fpu_temp));
    sys_workspace_save(outgoing->workspace_id);

    bool workspace_switched = sys_workspace_take_switch_request();
    if (workspace_switched) {
        sys_switch_terminal();
    }
    size_t next = scheduler_find_next(current_task);
    if (next == current_task) {
        if (workspace_switched) {
            scheduler_activate(current_task);
        } else {
            sys_workspace_load(outgoing->workspace_id,
                               outgoing->workspace_id ==
                                   (uint32_t)current_terminal_id);
        }
        __asm__ volatile ("fxrstor64 %0" : : "m"(outgoing->fpu_state));
        return frame;
    }
    scheduler_activate(next);
    return tasks[next].frame;
}

int scheduler_spawn_elf(const char *path, int argc, char **argv,
                        uint32_t workspace_id, uint32_t *out_pid) {
    if (path == NULL || path[0] == '\0' || out_pid == NULL ||
        argc < 0 || argc > 32 || (argc > 0 && argv == NULL) ||
        workspace_id >= MAX_TERMINALS) {
        return -1;
    }

    uint64_t flags = scheduler_disable_interrupts();
    size_t slot = SCHEDULER_TASK_LIMIT;
    for (size_t i = 1; i < SCHEDULER_TASK_LIMIT; i++) {
        if (!tasks[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == SCHEDULER_TASK_LIMIT) {
        scheduler_restore_interrupts(flags);
        return -1;
    }

    scheduler_task_t *task = &tasks[slot];
    scheduler_save_fs_context(&tasks[current_task]);
    memset(task, 0, sizeof(*task));
    task->workspace_id = workspace_id;
    task->drive = tasks[current_task].drive;
    task->fs_context_valid = tasks[current_task].fs_context_valid;
    memcpy(task->cwd, tasks[current_task].cwd, sizeof(task->cwd));

    uint64_t file_size;
    bool is_dir;
    if (fs_get_info((char *)path, &file_size, &is_dir) != 0 ||
        is_dir || file_size == 0 ||
        file_size > (uint64_t)MAX_FLAT_BINARY_SIZE) {
        scheduler_restore_fs_context(&tasks[current_task]);
        scheduler_restore_interrupts(flags);
        return -1;
    }

    uint64_t parent_cr3 = read_cr3() & ~0xFFFULL;
    uint64_t pml4 = create_user_pml4();
    if (pml4 == 0) {
        scheduler_restore_fs_context(&tasks[current_task]);
        scheduler_restore_interrupts(flags);
        return -1;
    }

    switch_user_pml4(pml4);
    uint64_t entry;
    uint64_t user_stack;
    uint64_t user_argv;
    uint64_t heap_base;
    bool linux_abi;
    
    int result = system_load_elf_args((char *)path, file_size, argc, argv,
                                      &entry, &user_stack, &user_argv,
                                      &linux_abi, &heap_base, &task->program);
    switch_user_pml4(parent_cr3);
    if (result != 0) {
        destroy_user_pml4(pml4);
        scheduler_restore_fs_context(&tasks[current_task]);
        scheduler_restore_interrupts(flags);
        return -1;
    }

    task->active = 1;
    task->pid = next_pid++;
    if (next_pid == 0) {
        next_pid = 2;
    }
    task->cr3 = pml4;
    task->gs_base = 0;
    task->kernel_gs_base = syscall_user_kernel_gs_base();
    task->linux_abi = linux_abi;
    task->frame = (interrupt_frame_t *)
        ((uint64_t)&task->kernel_stack[TASK_KERNEL_STACK_SIZE] -
         sizeof(interrupt_frame_t));
    memset(task->frame, 0, sizeof(*task->frame));
    task->frame->rdi = (uint64_t)argc;
    task->frame->rsi = user_argv;
    task->frame->vector = 32;
    task->frame->rip = entry;
    task->frame->cs = 0x23;
    task->frame->rflags = 0x202;
    task->frame->rsp = user_stack;
    task->frame->ss = 0x1B;
    linux_syscall_context_init(&task->linux_context);
    if (linux_abi) {
        if (heap_base > task->linux_context.heap_start &&
            heap_base < USER_MMAP_BASE) {
            task->linux_context.heap_start = heap_base;
            task->linux_context.program_break = heap_base;
        }
    }
    memcpy(task->fpu_state, tasks[0].fpu_state, sizeof(task->fpu_state));
    *out_pid = task->pid;
    scheduler_restore_fs_context(&tasks[current_task]);
    scheduler_restore_interrupts(flags);
    return 0;
}

size_t scheduler_list(scheduler_task_info_t *output, size_t capacity) {
    uint64_t flags = scheduler_disable_interrupts();
    size_t count = 0;
    for (size_t i = 1; i < SCHEDULER_TASK_LIMIT; i++) {
        if (!tasks[i].active) {
            continue;
        }
        if (output != NULL && count < capacity) {
            output[count].pid = tasks[i].pid;
            output[count].state = i == current_task ? 2U : 1U;
            output[count].workspace_id = tasks[i].workspace_id;
        }
        count++;
    }
    scheduler_restore_interrupts(flags);
    return count;
}

int scheduler_kill(uint32_t pid) {
    if (pid == 0) {
        return -1;
    }
    uint64_t flags = scheduler_disable_interrupts();
    for (size_t i = 1; i < SCHEDULER_TASK_LIMIT; i++) {
        if (tasks[i].active && tasks[i].pid == pid) {
            uint64_t pml4 = tasks[i].cr3;
            tasks[i].active = 0;
            cleanup_program_pages(&tasks[i].program);
            destroy_user_pml4(pml4);
            scheduler_restore_interrupts(flags);
            return 0;
        }
    }
    scheduler_restore_interrupts(flags);
    return -1;
}

uint32_t scheduler_current_pid(void) {
    return tasks[current_task].pid;
}

uint32_t scheduler_current_workspace(void) {
    return tasks[current_task].workspace_id;
}

void scheduler_set_shell_workspace(uint32_t workspace_id) {
    if (workspace_id >= MAX_TERMINALS) {
        return;
    }
    uint64_t flags = scheduler_disable_interrupts();
    tasks[0].workspace_id = workspace_id;
    scheduler_restore_interrupts(flags);
}

void scheduler_exit_current(uint64_t status) {
    (void)status;
    if (current_task == 0 || !tasks[current_task].active) {
        for (;;) {
            asm volatile ("cli; hlt");
        }
    }
    size_t exiting = current_task;
    scheduler_task_t *task = &tasks[exiting];
    if (syscall_linux_abi_active()) {
        linux_syscall_context_save(&task->linux_context);
    }
    scheduler_save_fs_context(task);
    sys_workspace_save(task->workspace_id);
    cleanup_program_pages(&task->program);
    uint64_t pml4 = task->cr3;
    task->active = 0;
    size_t next = scheduler_find_next(exiting);
    scheduler_activate(next);
    destroy_user_pml4(pml4);
    isr_resume_frame(tasks[next].frame);
}