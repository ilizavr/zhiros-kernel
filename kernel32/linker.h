/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../lib/zhirtypes.h"

extern API void* resolve_function(char * function_name);
extern API void register_function(char *function_name, void* call, char *description);
extern API bool load_mod(char diskletter,char *name);
extern API struct function_info * get_linker_head();
extern API struct module_info *get_module_array();
extern API bool replace_function(char* function_name, void* newfnc);
extern API bool unload_mod(u32 idx);
