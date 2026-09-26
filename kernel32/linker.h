/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../lib/zhirtypes.h"

extern API void* resolve_function(char * function_name);
extern API void register_function(char *function_name, void* call, char *description);
extern API bool load_mod(char diskletter,char *name);
extern API void help();
