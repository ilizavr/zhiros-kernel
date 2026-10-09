/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */

void (*printf)(...);

void _start(void* (*_resolve_function)(char* name))
{
    printf = _resolve_function("_printf");

    printf("hello world!\n");
}
