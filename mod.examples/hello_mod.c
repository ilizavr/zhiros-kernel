/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */


#include "../lib/zhirtypes.h"
#include "../lib/string.h"

void (*printf)(char* fmt, ...);

INIT void init(void* _resolve_function(char* name))
{
    printf = _resolve_function("_printf");

    printf("hello world from module!");
}
