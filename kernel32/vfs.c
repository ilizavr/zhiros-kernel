/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "vfs.h"
#include "allocator.h"
#include "shell/fbcon.h"
#include "../lib/string.h"

char current_letter_disk = 'A';

struct mount{
    struct disk* dsk;
    struct file* (*open)(struct disk* disk,char *path);
    struct file* (*mkdir)(struct disk* disk,char *path);
};
struct mount mounts[26];

char mount(struct disk* dsk, void * open_fnc, void * mkdir_fnc)
{
    u32 eflags = save_irq();
    if(current_letter_disk>'Z') {
        restore_irq(eflags);
        return 0;
    }

    mounts[current_letter_disk-'A'].dsk = dsk;
    mounts[current_letter_disk-'A'].open = open_fnc;
    mounts[current_letter_disk-'A'].mkdir = mkdir_fnc;
    current_letter_disk++;

    restore_irq(eflags);
    return current_letter_disk-1;
}

struct file* open(char disk_letter,char *path)
{
    if(disk_letter>='a'&&disk_letter<='z') disk_letter = disk_letter-'a'+'A';

    if(disk_letter<'A'||disk_letter>=current_letter_disk) return 0;
    if(!mounts[disk_letter-'A'].open) return 0;
    return mounts[disk_letter-'A'].open(mounts[disk_letter-'A'].dsk,path);
}

struct file* mkdir(char disk_letter,char *path)
{
    if(disk_letter>='a'&&disk_letter<='z') disk_letter = disk_letter-'a'+'A';

    if(disk_letter<'A'||disk_letter>=current_letter_disk) return 0;
    if(!mounts[disk_letter-'A'].mkdir) return 0;
    return mounts[disk_letter-'A'].mkdir(mounts[disk_letter-'A'].dsk,path);
}
/*
u32 read_stub(struct file* file, void* buffer, u32 size, u32 offset)
{
    file->dsk->lba_read(file->dsk,offset/512,buffer,size/512);
}
u32 getsize_stub(struct file* file)
{
    return file->dsk->size*512;
}
struct file* open_stub(struct disk* dsk,char *path)
{
    struct file* nf = kalloc(sizeof(struct file));
    nf->dsk = dsk;
    nf->read = read_stub;
    nf->getsize = getsize_stub;
    nf->write = 0;
    nf->close = 0;
    nf->is_dir = false;

    return nf;
}
*/
#include "unstar.h"
