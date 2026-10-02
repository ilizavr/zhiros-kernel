#include "../lib/zhirtypes.h"
#include "../lib/ports.h"
#include "interrupt.h"

#define FREQ 100

volatile u32 ticks = 0;

void sleep_ms(u32 ms)
{
    u32 t = ticks;
    while(ms>(ticks-t)) HLT();
}
u32 getticks()
{
    return ticks;
}

void inc_ticks()
{
    ticks+=(1000/FREQ);
}

void init_timer()
{
    u32 divisor = 1193182/FREQ;

    outb(0x43, 0x36);

    outb(0x40, divisor);
    outb(0x40, divisor>>8);

    hook_interrupt(0x20,inc_ticks);
}
