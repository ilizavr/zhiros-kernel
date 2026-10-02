/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "fbcon.h"
#include "../../fonts/font_6x8.h"
#include "../allocator.h"

i_ptr fb_addr;
u32 screen_width, screen_height, bpp, screen_pitch;
bool stopcon = false;

u32 fb_size,fb_size_withoutlastline,fb_lastline_addr,fb_onelinesize,fb_secondline_addr;

INLINE void put_pixel(u32 x, u32 y, u32 color) {
    if(stopcon) return;
    u32 *pixel = (u32*)(fb_addr + y * screen_pitch + x * bpp);
    *pixel &= 0xFF000000;
    if(bpp == 4)*pixel |= 0xFF000000;
    *pixel |= color;
}

INLINE void put_sym(u8 sym, u32 startx, u32 starty,u32 color,u32 bgcolor)
{
    if(stopcon) return;
    for(int y = 0;y<8;y++)
    {
        u8 font_row = font6x8[sym*8+y];
        for(int x = 0;x<6;x++)
        {
            if(font_row&0b10000000)
                put_pixel(startx+x,starty+y,color);
            else
                put_pixel(startx+x,starty+y,bgcolor);

            font_row<<=1;
        }
    }
}
INLINE void put_sym_nobg(u8 sym, u32 startx, u32 starty, u32 color)
{
    if (stopcon) return;
    for (int y = 0; y < 8; y++)
    {
        u8 font_row = font6x8[sym * 8 + y];
        for (int x = 6; x >= 0; x--)
        {
            put_pixel(startx + x, starty + y, (font_row&1)*color);
            font_row >>= 1;
        }
    }
}

void init_fbcon(i_ptr _fb_addr, u32 _screen_width, u32 _screen_height, u32 _screen_pitch, u32 _bbp)
{
    fb_addr = _fb_addr;
    screen_width = _screen_width/7*7;
    screen_height = _screen_height/9*9;
    screen_pitch = _screen_pitch;
    bpp = _bbp / 8;

    fb_size = screen_pitch*screen_height;
    fb_size_withoutlastline = (screen_height-9)*screen_pitch;
    fb_lastline_addr = fb_addr+(screen_height-9)*screen_pitch;
    fb_onelinesize = screen_pitch*9;
    fb_secondline_addr = fb_addr+screen_pitch*9;
}

u32 curx=0, cury=0;
void clearframe() {
    if(stopcon) return;
    cury = 0;curx = 0;
    memset((void*)fb_addr, 0, fb_size);
}


void roll_screen()
{
    if(stopcon) return;
    memmove((char*)fb_addr,(char*)fb_secondline_addr,fb_size_withoutlastline);
    memset((char*)fb_lastline_addr,0,fb_onelinesize);
}

void putchar_color(char chr, u32 color)
{
    if(stopcon) return;
    if(chr == '\n' || (curx+6)>screen_width)
    {
        cury+=9;
        curx=0;

        if((cury+9)>screen_height) {
            roll_screen();
            cury = screen_height-9;
        }
        if(chr == '\n')return;
    }
    if(chr == '\b')
    {
        if(curx>=7)curx-=7;
        else if(cury>=9){curx=screen_width-7;cury-=9;}
        return;
    }


    put_sym_nobg(chr,curx,cury,color);
    curx+=7;
}

void putchar(char chr)
{
    if(stopcon) return;
    putchar_color(chr, 0xAAAAAA);
}

void print_color(char *str, u32 color)
{
    if(stopcon) return;
    int len = strlen(str);
    for(int i = 0;i<len;i++)
    {
        putchar_color(str[i],color);
    }
}

void print(char *str)
{
    if(stopcon) return;
    print_color(str,0xAAAAAA);
}

void print_hex(u32 num)
{
    if(stopcon) return;
    char buffer[12]; buffer[11] = '\0';buffer[10] = '0';
    int i = 10;
    if(num==0)i--;
    for(;num>0;i--)
    {
        u64 nn = num %16;
        if(nn<10) buffer[i]=nn+'0';
        else buffer[i] = nn-0xa+'A';
        num>>=4;
    }
    print(buffer+i+1);
}

void print_int(u32 num)
{
    if(stopcon) return;
    char buffer[12]; buffer[11] = '\0';buffer[10] = '0';
    int i = 10;
    if(num==0)i--;
    for(;num>0;i--)
    {
        buffer[i] = num%10 +'0';
        num/=10;
    }

    print(buffer+i+1);
}
void fbcon_resume()
{
    stopcon = false;
}

struct fb_info* getfb()
{
    struct fb_info *ret = kalloc(sizeof(struct fb_info));
    ret->fb_addr = fb_addr;
    ret->screen_width = screen_width;
    ret->screen_height = screen_height;
    ret->screen_pitch = screen_pitch;
    ret->bpp = bpp;
    ret->curx = curx;
    ret->cury = cury;
    return ret;
}
struct fb_info* fbcon_stop()
{
    stopcon = true;

    return getfb();
}


