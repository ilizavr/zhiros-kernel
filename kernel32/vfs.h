/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "../lib/zhirtypes.h"
extern API char mount(struct disk* dsk, void * open_fnc, void * mkdir_fnc, void * create_file_fnc);
extern API struct file* open(char disk_letter,char *path);
extern API struct file* mkdir(char disk_letter,char *path);
extern API struct file* create_file(char disk_letter,char *path);
extern struct file* open_unstar(struct disk* dsk,char *path);
