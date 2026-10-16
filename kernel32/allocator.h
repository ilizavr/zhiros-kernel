/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../lib/zhirtypes.h"

API extern void *kalloc(u32 size);
API extern bool free(void *addr);
extern void allocator_add_page(i_ptr start, i_ptr end, i_ptr modulestart, i_ptr moduleend);
API extern i_ptr get_available_memory();
API extern i_ptr get_used_memory();
API extern void debug_heap();
