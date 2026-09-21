/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// only 32bit support

#include "interrupt.h"
#include "../lib/ports.h"
#include "panic.h"

void pic_remap()
{
	//init master and slave
	outb(0x20,0x11);
	outb(0xA0,0x11);
	
	//mov irq to 0x20-0x2F
	outb(0x21,0x20);
	outb(0xA1,0x28);

	//master-slave connection
	outb(0x21,0x04);
	outb(0xA1,0x02);

	//8086 mode
	outb(0x21,0x01);
	outb(0xA1,0x01);

	//enable interrupts
	outb(0x21,0);
	outb(0xA1,0);
}
void pic_eoi(u32 irq)
{
	if(irq>=8) outb(0xA0, 0x20);
	outb(0x20, 0x20);
}


PAK struct idt_entry
{
	u16 low_offset;
	u16 sel;
	u8 always0;
	u8 flags;
	u16 high_offset;
};
PAK struct idt_ptr
{
	u16 limit;
	u16 base_low;
	u16 base_high;
};

struct idt_entry idt[256];
struct idt_ptr idtp;


void set_idt_gate(u8 num,void* fnc)
{
	u32 base = (u32)fnc;
	idt[num].low_offset = base&0xFFFF;
	idt[num].high_offset = (base>>16)&0xFFFF;
	idt[num].sel = 0x10;
	idt[num].always0 = 0;
	idt[num].flags = 0x8E;//ring0 32bit
}

extern struct
{
	char buffer[64];
	void* hooks[16];
} _interrupt_array[];

void interrupt_handler(u32 num)
{
	for(int i = 0;i<16;i++)
		if(_interrupt_array[num].hooks[i])
			CALL(_interrupt_array[num].hooks[i]);

	if(num<32) kernel_panic(num);
	else if(num<0x30) pic_eoi(num-0x20);
}


bool hook_interrupt(u32 num, void* fnc)
{
	for(int i = 0;i<16;i++)
		if(!_interrupt_array[num].hooks[i]){
			_interrupt_array[num].hooks[i] = fnc;
			return true;
		}
	return false;
}

void init_idt()
{
	idtp.limit = 256*sizeof(struct idt_entry)-1;
	idtp.base_low = (u16)&idt;
	idtp.base_high = (u32)&idt>>16;

	for(int i = 0;i<256;i++) set_idt_gate(i,&_interrupt_array[i]);

	asm volatile("lidt (%0)" : : "r" (&idtp));
}
