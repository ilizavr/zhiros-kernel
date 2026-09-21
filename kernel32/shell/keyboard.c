/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "keyboard.h"
#include "../interrupt.h"
#include "../../lib/ports.h"
#include "fbcon.h"

volatile bool key_state_matrix[256] = {0};

bool shift_pressed = false;
bool right_shift_pressed = false;

#define KB_BUF_SIZE 64
#define KB_BUF_MASK 63

char kb_buffer[KB_BUF_SIZE];
u32 kb_head = 0, kb_tail = 0;


void keyboard_isr_handler()
{
    u8 key = inb(0x60);
    if(key==0x2A)shift_pressed=true;
    if(key==0xAA)shift_pressed=false;
    if(key==0x36)right_shift_pressed=true;
    if(key==0xB6)right_shift_pressed=false;

    if((key&0x80) == 0)
    {
        key_state_matrix[key&0x7F] = 1;

        if(!keyboard_map[key&0x7F]) return;

        u32 kb_head_next = (kb_head+1)&KB_BUF_MASK;//защита от переполнения кольцевого буфера
        if(kb_head_next == kb_tail) return;

        if(shift_pressed||right_shift_pressed)kb_buffer[kb_head] = keyboard_map_shift[key&0x7F];
        else kb_buffer[kb_head] = keyboard_map[key&0x7F];
        kb_head = kb_head_next;
    }else key_state_matrix[key&0x7F] = 0;
}

bool* get_key_state_matrix()
{
    return key_state_matrix;
}

char getchar()
{
    while(kb_head == kb_tail) HLT();
    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail+1)&KB_BUF_MASK;
    return c;
}

u32 gets(char *str, u32 maxlen)
{
    u32 i = 0;
    while(i<(maxlen-1))
    {
        char chr = getchar();

        if(chr == '\n') {
            break;
        }
        if(chr == '\b')
        {
            if(i>0){
                print("\b \b");
                i--;
            }
            continue;
        }
        putchar(chr);
        str[i++] = chr;
    }
    str[i] = 0;
    return i;
}

void init_keyboard()
{
    hook_interrupt(33, keyboard_isr_handler);
}
