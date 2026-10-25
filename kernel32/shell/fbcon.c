/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "fbcon.h"
#include "../../fonts/font_6x8.h"
#include "../allocator.h"

volatile i_ptr real_fb_addr;
volatile i_ptr fb_addr;
volatile u32 screen_width, screen_height, bpp, screen_pitch;
volatile bool stopcon = false;

volatile u32 fb_size,fb_size_withoutlastline,fb_lastline_addr,fb_onelinesize,fb_secondline_addr;

volatile u32 fb_scale = 2;

void set_font_scale(u32 scale) {
    scale++;
    if (scale < 2 || scale > 6) scale = 2;
    fb_scale = scale;

    fb_size_withoutlastline = (screen_height - 9*fb_scale/2) * screen_pitch;
    fb_lastline_addr = fb_addr + (screen_height - 9*fb_scale/2) * screen_pitch;
    fb_onelinesize = screen_pitch * 9*fb_scale/2;
    fb_secondline_addr = fb_addr + screen_pitch * 9*fb_scale/2;
}


INLINE void put_pixel(u32 x, u32 y, u32 color) {
    u32 *pixel = (u32*)(fb_addr + y * screen_pitch + x * bpp);
    *pixel = color|0xFF000000;

    u32 *realpixel = (u32*)(real_fb_addr + y * screen_pitch + x * bpp);
    *realpixel = *pixel;

}

INLINE void put_sym(u8 sym, u32 startx, u32 starty,u32 color,u32 bgcolor)
{
    for(int y = 0;y<8;y++)
    {
        u8 font_row = font6x8[sym*8+y];
        for(int x = 0;x<6;x++)
        {
            u32 col = bgcolor;
            if(font_row&0b10000000)
                col = color;

            for (u32 dy = 0; dy < fb_scale; dy++) {
                for (u32 dx = 0; dx < fb_scale; dx++) {
                    put_pixel(startx + (x * fb_scale + dx)/2, starty + (y * fb_scale + dy)/2, col);
                }
            }

            font_row<<=1;
        }
    }
}
INLINE void put_sym_nobg(u8 sym, u32 startx, u32 starty, u32 color)
{
    put_sym(sym,startx, starty,color,0);
}

void init_fbcon(i_ptr _fb_addr, u32 _screen_width, u32 _screen_height, u32 _screen_pitch, u32 _bbp)
{
    real_fb_addr = _fb_addr;
    fb_addr = 0x03000000;
    screen_width = _screen_width/7*7;
    screen_height = _screen_height/9*9;
    screen_pitch = _screen_pitch;
    bpp = _bbp / 8;

    fb_size = screen_pitch*screen_height;

    fb_size_withoutlastline = (screen_height-9)*screen_pitch;
    fb_lastline_addr = fb_addr+(screen_height-9)*screen_pitch;
    fb_onelinesize = screen_pitch*9;
    fb_secondline_addr = fb_addr+screen_pitch*9;

    clearframe();
}

u32 curx=0, cury=0;
void clearframe() {
    if(stopcon) return;
    cury = 0;curx = 0;
    memset((void*)fb_addr, 0, fb_size);
    memset((void*)real_fb_addr, 0, fb_size);
}


void roll_screen()
{
    memcpy_wc((char*)fb_addr,(char*)fb_secondline_addr,fb_size_withoutlastline);
    memset((char*)fb_lastline_addr,0,fb_onelinesize);

    memcpy_wc((void*)real_fb_addr,(void*)fb_addr, fb_size);
}

void putchar_color(char chr, u32 color)
{
    if(stopcon) return;
    u32 char_w = 7 * fb_scale /2;
    u32 char_h = 9 * fb_scale /2;

    if((curx+char_w-1)>screen_width)
    {
        cury+=char_h;
        curx=0;

        if((cury+char_h)>screen_height) {
            roll_screen();
            cury = screen_height-char_h;
        }
    }
    if(chr == '\b')
    {
        if(curx>=char_w)curx-=char_w;
        else if(cury>=char_h){curx=screen_width-char_w;cury-=char_h;}
        return;
    }


    if(chr == '\n')
    {
        put_sym_nobg(' ',curx,cury,color);
        // memcpy_wc((void*)(real_fb_addr + cury * screen_pitch), (void*)(fb_addr + cury * screen_pitch), fb_onelinesize);

        cury+=char_h;
        curx=0;

        if((cury+char_h)>screen_height) {
            roll_screen();
            cury = screen_height-char_h;
        }
        return;
    }

    put_sym_nobg(chr,curx,cury,color);

    // memcpy_wc((void*)(real_fb_addr + cury * screen_pitch), (void*)(fb_addr + cury * screen_pitch), fb_onelinesize);
    curx+=char_w;
}

O0 void print_color(char *str, u32 color)
{
    int len = strlen(str);
    for(int i = 0;i<len;i++)
    {
        putchar_color(str[i],color);
    }
}

void print_hex(u32 num,u32 color)
{
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
    print_color(buffer+i+1,color);
}

void print_int(u32 num,u32 color)
{
    char buffer[12]; buffer[11] = '\0';buffer[10] = '0';
    int i = 10;
    if(num==0)i--;
    for(;num>0;i--)
    {
        buffer[i] = num%10 +'0';
        num/=10;
    }

    print_color(buffer+i+1,color);
}
void fbcon_resume()
{
    stopcon = false;
}

struct fb_info* getfb()
{
    struct fb_info *ret = kalloc(sizeof(struct fb_info));
    ret->fb_addr = real_fb_addr;
    ret->shadow_fb_addr = fb_addr;
    ret->screen_width = screen_width;
    ret->screen_height = screen_height;
    ret->screen_pitch = screen_pitch;
    ret->bpp = bpp;
    ret->curx = curx;
    ret->cury = cury;
    ret->font_scale = fb_scale;
    return ret;
}
struct fb_info* fbcon_stop()
{
    stopcon = true;

    return getfb();
}

