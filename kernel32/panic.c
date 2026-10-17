/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "panic.h"
#include "shell/fbcon.h"
#include "../lib/string.h"

const char *exception_names[32] = {
    [0]  = "Divide-by-zero Error (#DE)",
    [1]  = "Debug (#DB)",
    [2]  = "Non-maskable Interrupt (#NMI)",
    [3]  = "Breakpoint (#BP)",
    [4]  = "Overflow (#OF)",
    [5]  = "BOUND Range Exceeded (#BR)",
    [6]  = "Invalid Opcode (#UD)",
    [7]  = "Device Not Available (#NM)",
    [8]  = "Double Fault (#DF)",
    [9]  = "Coprocessor Segment Overrun",
    [10] = "Invalid TSS (#TS)",
    [11] = "Segment Not Present (#NP)",
    [12] = "Stack-Segment Fault (#SS)",
    [13] = "General Protection Fault (#GP)",
    [14] = "Page Fault (#PF)",
    [15] = "Reserved",
    [16] = "x87 Floating-Point Exception (#MF)",
    [17] = "Alignment Check (#AC)",
    [18] = "Machine Check (#MC)",
    [19] = "SIMD Floating-Point Exception (#XM)",
    [20] = "Virtualization Exception (#VE)",
    [21] = "Control Protection Exception (#CP)",
    [22] = "Reserved",
    [23] = "Reserved",
    [24] = "Reserved",
    [25] = "Reserved",
    [26] = "Reserved",
    [27] = "Reserved",
    [28] = "Hypervisor Injection Exception (#HV)",
    [29] = "VMM Communication Exception (#VC)",
    [30] = "Security Exception (#SX)",
    [31] = "Reserved"
};

#define MEMORY_ERR 32

char *sadbear[]={
    "        (()__(()",
    "        /       \\",
    "       ( /    \\  \\",
    "        \\ o o    /",
    "        (_()_)__/ \\",
    "       / _,==.____ \\",
    "      (   |--|      )",
    "      /\\_.|__|'-.__/\\_",
    "     / (        /     \\",
    "     \\  \\      (      /",
    "      )  '._____)    /",
    "   (((____.--(((____/",
};

char * panic_text[]={
    " _  _______ ____  _   _ _____ _",
    "| |/ / ____|  _ \\| \\ | | ____| |    ",
    "| ' /|  _| | |_) |  \\| |  _| | |    ",
    "| . \\| |___|  _ <| |\\  | |___| |___ ",
    "|_|\\_\\_____|_| \\_\\_| \\_|_____|_____|",
    "",
    " ____   _    _   _ ___ ____ ",
    "|  _ \\ / \\  | \\ | |_ _/ ___|",
    "| |_) / _ \\ |  \\| || | |    ",
    "|  __/ ___ \\| |\\  || | |___ ",
    "|_| /_/   \\_\\_| \\_|___\\____|",
};

void print_bear()
{
    for(int i = 0;i<12;i++) {
        print_color(sadbear[i],0xFF5500);
        if(i<11){
            for(int j = strlen(sadbear[i]);j<24;j++)printf(" ");
            if(i>5) print_color(panic_text[i],0xFF0000);
            else printf(panic_text[i]);
        }
        printf("\n");
    }
    printf("\n");
}


void kernel_panic(u32 cpu_exception)
{
    fbcon_resume();
    CLI();

    int stack[0];
    clearframe();

    print_bear();

    printf("%s\n\nSTACK:",exception_names[cpu_exception]);
    for(int i = 0;i<32;i++)
    {
        printf("%x ",stack[i]);
    }
    while(true) HLT();
}

void kernel_panic_text(char * text)
{
    fbcon_resume();
    CLI();

    int stack[0];
    clearframe();

    print_bear();

    printf("%s\n\nSTACK:",text);
    for(int i = 0;i<32;i++)
    {
        printf("%x ",stack[i]);
    }
    while(true) HLT();
}
