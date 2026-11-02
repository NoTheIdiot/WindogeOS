#ifndef BOOT_TASK_INFO_H
#define BOOT_TASK_INFO_H

#include <stdint.h>

#define SCHEDULER_TASK_LIMIT 8

typedef struct {
    uint32_t pid;
    uint32_t state;
    uint32_t workspace_id;
} scheduler_task_info_t;

#endif
