/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "allocator.h"
#include "panic.h"

extern char _kernel_end[];
extern char _kernel_start[];

#define alignment(size) (((size) + 15) & ~15)
#define alignment_(size) ((size) & ~15)

PAK struct memoryblock
{
    u32 size;
    u32 allocated;

    struct memoryblock *prev;
    struct memoryblock *next;
};

struct memoryblock headblock = {0,1,0,0};
struct memoryblock *footblock = &headblock;


void *kalloc(u32 size)
{
    u32 eflags = save_irq();
    size=alignment(size);
    struct memoryblock *current = headblock.next;

    while(current)
    {
        if(current->size >= size && !current->allocated)
        {
            if((current->size - size) > 64)
            {
                struct memoryblock* newblock = (struct memoryblock*)((u32)current+sizeof(struct memoryblock)+size);
                newblock->next = current->next;
                current->next=newblock;
                if(newblock->next) newblock->next->prev = newblock;
                newblock->prev=current;
                newblock->size=current->size-size-sizeof(struct memoryblock);
                current->size=size;
                newblock->allocated = 0;
            }
            current->allocated = 1;
            restore_irq(eflags);
            return (void*)((u32)current+sizeof(struct memoryblock));
        }
        current = current->next;
    }

    restore_irq(eflags);
    return 0;

}


bool free(void *addr)
{
    u32 eflags = save_irq();
    if(!addr) {
        restore_irq(eflags);
        return false;
    }
    struct memoryblock *block = (struct memoryblock*)((u32)addr - sizeof(struct memoryblock));
    if(!block->allocated){
        restore_irq(eflags);
        return false;
    }
    block->allocated = 0;

    if((i_ptr)block->next == ((i_ptr)block+block->size+sizeof(struct memoryblock)) && block->next->allocated == 0)
    {
        block->size+=block->next->size+sizeof(struct memoryblock);
        if(block->next->next)block->next->next->prev = block;
        block->next=block->next->next;
    }
    if(block->prev&&block->prev->allocated == 0)
    {
        block=block->prev;

        if((i_ptr)block->next != ((i_ptr)block+block->size+sizeof(struct memoryblock))) goto END;

        block->size+=block->next->size+sizeof(struct memoryblock);
        if(block->next->next)block->next->next->prev = block;
        block->next=block->next->next;
    }
END:
    restore_irq(eflags);
    return true;
}

void _allocator_add_page(i_ptr start, i_ptr end)
{
    start = alignment(start);
    if(start == 0) start = 0x10;
    end = alignment_(end);

    i_ptr size = end-start;
    if(end<=start || size <= 256) return;

    footblock->next = (struct memoryblock*)start;
    footblock->next->prev = footblock;
    footblock->next->size = size - sizeof(struct memoryblock);
    footblock->next->allocated = 0;
    footblock->next->next = 0;

    footblock = footblock->next;
}

void allocator_add_page(i_ptr start, i_ptr end, i_ptr modulestart, i_ptr moduleend)
{
    i_ptr k_start = (i_ptr)&_kernel_start;
    i_ptr k_end = (i_ptr)&_kernel_end;

    if (start < k_end && end > k_start)
    {
        if (start < k_start) allocator_add_page(start, k_start, modulestart, moduleend);
        if (end > k_end) allocator_add_page(k_end, end, modulestart, moduleend);
        return;
    }

    if (start < 0x100000)
    {
        if (end > 0x100000) allocator_add_page(0x100000, end, modulestart, moduleend);
        return;
    }

    if (start < moduleend && end > modulestart) {
        if (start < modulestart) allocator_add_page(start, modulestart, modulestart, moduleend);
        if (end > moduleend) allocator_add_page(moduleend, end, modulestart, moduleend);
        return;
    }

    _allocator_add_page(start,end);
}

i_ptr get_available_memory()
{
    i_ptr available_memory = 0;
    u32 eflags = save_irq();
    struct memoryblock *current = headblock.next;

    while(current)
    {
        available_memory += current->size;
        current = current->next;
    }

    restore_irq(eflags);
    return available_memory;
}
