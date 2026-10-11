/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "disk.h"
#include "../lib/string.h"
#include "allocator.h"


volatile struct disk* disks[MAX_DISK_COUNT] = {0};
volatile u8 disks_count = 0;


bool ramdisk_read(struct disk* dsk, u32 lba, char* buffer, u32 blocks)
{
    if ((lba + blocks) > dsk->size) {
        return false;
    }
    memcpy(buffer,dsk->other_info+lba*512,blocks*512);
    return true;
}

bool ramdisk_write(struct disk* dsk, u32 lba, char* buffer, u32 blocks)
{
    if ((lba + blocks) > dsk->size) {
        return false;
    }
    memcpy(dsk->other_info+lba*512,buffer,blocks*512);
    return true;
}

API bool diskadd(struct disk* dsk)
{
    u32 eflags = save_irq();

    if(disks_count>=MAX_DISK_COUNT) {
        restore_irq(eflags);
        return false;
    }

    disks[disks_count] = dsk;
    disks_count++;
    restore_irq(eflags);

    return true;
}

API struct disk* getdisk(int idx)
{
    if(idx>=disks_count)return 0;
    return disks[idx];
}

struct disk* ramdisk_init(i_ptr modulestart, i_ptr moduleend)
{
    struct disk* newramdisk = kalloc(sizeof(struct disk)+8);
    newramdisk->lba_read = ramdisk_read;
    newramdisk->lba_write = ramdisk_write;
    newramdisk->size = (moduleend-modulestart)/512+1;
    newramdisk->name = "ramdisk";
    newramdisk->other_info = (void*)modulestart;
    newramdisk->mountletter=0;

    if(!diskadd(newramdisk)) return 0;
    return newramdisk;
}
