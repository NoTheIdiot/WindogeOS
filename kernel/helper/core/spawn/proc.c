#include <process.h>
#include <system.h>
#include <string.h>

#define USER_STACK_TOP   0x00007FFFFFFF0000ULL
#define USER_STACK_PAGES 16

static uint64_t g_next_pid = 1;

process *current_proc = NULL;
extern uint64_t hhdm_offset;

bool proc_create_stack(process *proc)
{
    if (!proc)
        return false;

    uint64_t stack_size =
        USER_STACK_PAGES * PAGE_SIZE;

    uint64_t stack_base =
        USER_STACK_TOP - stack_size;

    for (uint64_t addr = stack_base;
         addr < USER_STACK_TOP;
         addr += PAGE_SIZE)
    {
        uintptr_t phys =
            alloc_page();

        if (!phys)
            return false;

        memset(
            (void *)(phys + hhdm_offset),
            0,
            PAGE_SIZE);

        if (!map_page(
                addr,
                phys,
                PAGE_PRESENT |
                PAGE_WRITABLE |
                PAGE_USER))
        {
            free_page(phys);
            return false;
        }
    }

    proc->stack_top = USER_STACK_TOP;

    if (proc->region_count <
        MAX_PROC_REGIONS)
    {
        vm_region_t *region =
            &proc->regions[
                proc->region_count++];

        region->start = stack_base;
        region->end   = USER_STACK_TOP;
        region->flags =
            PAGE_PRESENT |
            PAGE_WRITABLE |
            PAGE_USER;
    }

    return true;
}

bool proc_push64(
    process *proc,
    uint64_t value)
{
    if (!proc)
        return false;

    proc->stack_top -= sizeof(uint64_t);

    *(uint64_t *)
        proc->stack_top = value;

    return true;
}

uint64_t proc_push_data(
    process *proc,
    const void *data,
    uint64_t size)
{
    if (!proc || !data)
        return 0;

    proc->stack_top -= size;

    memcpy(
        (void *)proc->stack_top,
        data,
        size);

    return proc->stack_top;
}

static uint64_t str_len(
    const char *s)
{
    uint64_t len = 0;

    while (s[len])
        len++;

    return len;
}

uint64_t proc_push_string(
    process *proc,
    const char *str)
{
    return proc_push_data(
        proc,
        str,
        str_len(str) + 1);
}

uint64_t proc_build_argv(
    process *proc,
    int argc,
    char **argv)
{
    uint64_t argv_ptrs[64];

    if (!proc)
        return 0;

    if (argc < 0)
        argc = 0;

    if (argc > 64)
        argc = 64;

    for (int i = argc - 1;
         i >= 0;
         i--)
    {
        argv_ptrs[i] =
            proc_push_string(
                proc,
                argv[i]);
    }

    proc->stack_top &= ~0xFULL;

    proc_push64(proc, 0);

    for (int i = argc - 1;
         i >= 0;
         i--)
    {
        proc_push64(
            proc,
            argv_ptrs[i]);
    }

    return proc->stack_top;
}

bool proc_grow_stack(
    process *proc,
    uint64_t fault_addr)
{
    if (!proc)
        return false;

    uint64_t page =
        fault_addr &
        ~(PAGE_SIZE - 1ULL);

    uintptr_t phys =
        alloc_page();

    if (!phys)
        return false;

    memset(
        (void *)(phys + hhdm_offset),
        0,
        PAGE_SIZE);

    if (!map_page(
            page,
            phys,
            PAGE_PRESENT |
            PAGE_WRITABLE |
            PAGE_USER))
    {
        free_page(phys);
        return false;
    }

    for (size_t i = 0;
         i < proc->region_count;
         i++)
    {
        vm_region_t *r =
            &proc->regions[i];

        if (r->end ==
            USER_STACK_TOP)
        {
            if (page < r->start)
                r->start = page;

            break;
        }
    }

    return true;
}

uint64_t allocate_pid(void)
{
    return g_next_pid++;
}