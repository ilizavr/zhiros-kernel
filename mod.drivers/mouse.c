/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */

#include "../lib/zhirapi.h"
#include "../lib/string.h"
#include "../lib/ports.h"

void wait_input_clear() {
    u32 timeout = 10000000;
    while ((inb(0x64) & 0x02) && timeout--);
}

void wait_output_full() {
    u32 timeout = 10000000;
    while (!(inb(0x64) & 0x01) && timeout--);
}

u8 mouse_read_status()
{
    wait_output_full();
    return inb(0x64);
}

void mouse_send_cmd(u8 cmd)
{
    wait_input_clear();
    outb(0x64, cmd);
}

void mouse_send_data(u8 data) {
    mouse_send_cmd(0xD4);
    wait_input_clear();
    outb(0x60, data);
}

u8 mouse_read_data() {
    while (true){
        if (mouse_read_status() & 0x20) {
            wait_output_full();
            return inb(0x60);
        }
        (void)inb(0x60);
    }

    return 0;
}

bool init_mouse() {
    //enable mouse
    mouse_send_cmd(0xA8);

    //enable interrupt
    mouse_send_cmd(0x20);
    wait_output_full();//зависает на qemu
    u8 config = inb(0x60);

    config |= 0x02; //IRQ12
    config &= ~0x20; //enable mouse

    mouse_send_cmd(0x60);
    wait_input_clear();
    outb(0x60, config);

    //enable packet
    mouse_send_data(0xF4);
    mouse_read_data();

    LOGI("mouse inited");
    return true;
}

volatile u8 cycle = 0;
volatile u8 packet[3];

volatile s32 mouse_x = 100;
volatile s32 mouse_y = 100;
volatile bool mouse_left = false;
volatile bool mouse_left_old = false;
volatile bool mouse_right = false;

void * fncs[64];
u32 curfnc = 0;

static void mouse_irq_handler() {
    packet[cycle] = mouse_read_data();

    // allign check
    if (cycle == 0 && !(packet[cycle] & 0x08)) return;

    cycle++;

    if (cycle == 3) {
        cycle = 0;

        mouse_left = packet[0] & 0x01;
        mouse_right = packet[0] & 0x02;

        s16 dx = packet[1];
        s16 dy = packet[2];

        if (packet[0] & 0x10) dx |= 0xFF00;
        if (packet[0] & 0x20) dy |= 0xFF00;

        mouse_x += dx;
        mouse_y -= dy;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x >= 800) mouse_x = 799;
        if (mouse_y >= 600) mouse_y = 599;

        if(mouse_left && !mouse_left_old) for(int i = 0;i<curfnc;i++)
        {
            CALL(fncs[i],mouse_x,mouse_y);
        }
        mouse_left_old = mouse_left;
    }
}

static s32 get_mouse_x()
{
    return mouse_x;
}

static s32 get_mouse_y()
{
    return mouse_y;
}

static bool get_mouse_left()
{
    return mouse_left;
}

static bool get_mouse_right()
{
    return mouse_right;
}

static bool register_click_event(void * fnc)
{
    if(curfnc>=63) return false;
    fncs[curfnc++] = fnc;
}

INIT void init(void* resolver)
{
    api_init(resolver);
    if(init_mouse()){
        if(!hook_interrupt(0x2C, mouse_irq_handler))LOGE("error hook interrupt");

        register_function("_get_mouse_x",get_mouse_x,"_get_mouse_x() -> s32");
        register_function("_get_mouse_y",get_mouse_y,"_get_mouse_y() -> s32");
        register_function("_get_mouse_left",get_mouse_left,"_get_mouse_left() -> bool");
        register_function("_get_mouse_right",get_mouse_right,"_get_mouse_right() -> bool");

        register_function("_register_click_event",register_click_event,"_register_click_event(function event) -> bool");
    }
}
