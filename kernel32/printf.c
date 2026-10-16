/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "printf.h"
#include "shell/fbcon.h"

void printf(char *fmt, ...)
{
    va_list args;
    va_start(args,fmt);

    for(;*fmt;fmt++)
    {
        if(*fmt != '%'){
            putchar(*fmt);
            continue;
        }

        fmt++;
        if(!*fmt) break;

        switch (*fmt){
            case 's':{
                char *str = va_arg(args, char*);
                if(str) print(str);
                break;
            }
            case 'c':{
                u32 chr = va_arg(args, u32);
                putchar(chr&0xFF);
                break;
            }
            case 'u':{
                u32 num = va_arg(args, u32);
                print_int(num);
                break;
            }
            case 'x':{
                u32 num = va_arg(args, u32);
                print_hex(num);
                break;
            }
            case 'Z':{
                u64 num = va_arg(args, u32);
                if(num>=_1M) {
                    print_int(num>>20);
                    putchar('G');
                    break;
                }
                if(num>=_1K) {
                    print_int(num>>10);
                    putchar('M');
                    break;
                }

                print_int(num);
                putchar('k');

                break;
            }
            case '%':{
                putchar('%');
                break;
            }
            default:{
                putchar('%');
                putchar(*fmt);
                break;
            }
        }

    }
}
