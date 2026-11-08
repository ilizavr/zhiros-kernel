/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */

#include "../lib/zhirapi.h"

void _start(void* resolve)
{
    api_init(resolve);
    printf("hello world!\n");
}
