/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "vfs.h"
#include "allocator.h"
#include "shell/fbcon.h"
#include "../lib/string.h"

char current_letter_disk = 'A';

struct mount mounts[26];

char mount(struct disk* dsk, void * open_fnc, void * mkdir_fnc, void * create_file_fnc)
{
    u32 eflags = save_irq();
    if(current_letter_disk>'Z') {
        restore_irq(eflags);
        return 0;
    }

    mounts[current_letter_disk-'A'].dsk = dsk;
    mounts[current_letter_disk-'A'].open = open_fnc;
    mounts[current_letter_disk-'A'].mkdir = mkdir_fnc;
    mounts[current_letter_disk-'A'].create_file = create_file_fnc;
    current_letter_disk++;

    dsk->mountletter = current_letter_disk-1;
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

struct file* create_file(char disk_letter,char *path)
{
    if(disk_letter>='a'&&disk_letter<='z') disk_letter = disk_letter-'a'+'A';

    if(disk_letter<'A'||disk_letter>=current_letter_disk) return 0;
    if(!mounts[disk_letter-'A'].create_file) return 0;
    return mounts[disk_letter-'A'].create_file(mounts[disk_letter-'A'].dsk,path);
}

#include "unstar.h"
