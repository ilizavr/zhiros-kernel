/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../lib/zhirtypes.h"

extern void pic_remap();
extern void init_idt();
extern API bool hook_interrupt(u32 num, void* fnc);
