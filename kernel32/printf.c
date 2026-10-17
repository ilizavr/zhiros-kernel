/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "printf.h"
#include "shell/fbcon.h"

volatile u32 printfcolor = 0xAAAAAA;

void printf(char *fmt, ...)
{
    va_list args;
    va_start(args,fmt);

    for(;*fmt;fmt++)
    {
        if(*fmt != '%'){
            putchar_color(*fmt,printfcolor);
            continue;
        }

        fmt++;
        if(!*fmt) break;

        switch (*fmt){
            case 's':{
                char *str = va_arg(args, char*);
                if(str) print_color(str,printfcolor);
                break;
            }
            case 'c':{
                u32 chr = va_arg(args, u32);
                putchar_color(chr&0xFF,printfcolor);
                break;
            }
            case 'C':{
                printfcolor = va_arg(args, u32);
                break;
            }
            case 'u':{
                u32 num = va_arg(args, u32);
                print_int(num,printfcolor);
                break;
            }
            case 'x':{
                u32 num = va_arg(args, u32);
                print_hex(num,printfcolor);
                break;
            }
            case 'Z':{
                u32 num = va_arg(args, u32);
                if(num>=_1M) {
                    print_int(num/1000000,printfcolor);
                    putchar_color('.',printfcolor);
                    print_int(num/100000%10,printfcolor);
                    putchar_color('G',printfcolor);
                    break;
                }
                if(num>=_1K) {
                    print_int(num/1000,printfcolor);
                    putchar_color('.',printfcolor);
                    print_int(num/100%10,printfcolor);
                    putchar_color('M',printfcolor);
                    break;
                }

                print_int(num,printfcolor);
                putchar_color('k',printfcolor);

                break;
            }
            case '%':{
                putchar_color('%',printfcolor);
                break;
            }
            default:{
                putchar_color('%',printfcolor);
                putchar_color(*fmt,printfcolor);
                break;
            }
        }

    }
}
