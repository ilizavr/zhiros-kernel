/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "../lib/zhirtypes.h"
#include "../lib/string.h"
#include "interrupt.h"
#include "allocator.h"
#include "multitask.h"

struct task tasks[MAX_TASKS] = {0};
int current_task_idx = 0;
volatile bool preemptive_mode = false;

static char *strdup_c(char *str)
{
    u32 len = strlen(str);
    char * newstr = kalloc(len+1);
    memcpy(newstr,str,len+1);
    return newstr;
}

extern void switch_context(void* sp, void** oldsp);

struct task* create_task(char *name, void* function)
{
    for (int i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].tid == 0)
        {
            tasks[i].tid = i + 1;
            tasks[i].name = strdup_c(name);
            tasks[i].stack_base = kalloc(STACK_SIZE);

            u32 *esp = (u32*)(tasks[i].stack_base + STACK_SIZE);

            *(--esp) = (u32)function;

            // popa EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX
            *(--esp) = 0; // EDI
            *(--esp) = 0; // ESI
            *(--esp) = 0; // EBP
            *(--esp) = 0; // ESP(ignore)
            *(--esp) = 0; // EBX
            *(--esp) = 0; // EDX
            *(--esp) = 0; // ECX
            *(--esp) = 0; // EAX

            tasks[i].esp = esp;

            return &tasks[i];
        }
    }

    return 0;
}

bool kill(int tid)
{
    if(tid<=0 || tasks[tid-1].tid != tid) return false;

    tasks[tid-1].tid = 0;
    free(tasks[tid-1].stack_base);

    return true;
}

void yield()
{
    int next_idx = -1;
    int start = current_task_idx;

    for (int i = 1; i <= MAX_TASKS; i++) {
        int idx = (start + i) % MAX_TASKS;
        if (tasks[idx].tid != 0) {
            next_idx = idx;
            break;
        }
    }

    if (next_idx == -1 || next_idx == current_task_idx) {
        return;
    }

    int prev_idx = current_task_idx;
    current_task_idx = next_idx;

    switch_context(tasks[current_task_idx].esp, &tasks[prev_idx].esp);
}


struct task* get_tasks()
{
    return tasks;
}

void init_multitask()
{
    tasks[0].tid = 1;
    tasks[0].name = "init";
    tasks[0].stack_base = 0;
    tasks[0].esp = 0;
    current_task_idx = 0;
}
