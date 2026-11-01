/*
 * Copyright (c) 2026 ilizavr & TINERKOTL & yellowhat
 * SPDX-License-Identifier: MIT
 */

#include "../lib/zhirapi.h"
#include "WM_types.h"

static Window* (*window_create)(char *name);
static void (*window_render)(Window *window);
static void (*clearframe_win)(Window *window, u32 color);
static void (*window_fill_rect)(Window *window, u32 x, u32 y, u32 width, u32 height, u32 color);
static void (*draw_string)(Window *window, const char *str, u32 x, u32 y, u32 color);
static void (*draw_char_bg)(Window *window, char chr, u32 x, u32 y, u32 color, u32 bgcolor);
static void (*draw_border_window)(Window *window, u32 color);
static void (*compositor_present)();
static bool (*window_is_active)(Window *window);
static void (*desktop_fill_rect)(s32 x, s32 y, s32 width, s32 height, u32 color);
static void (*desktop_draw_string)(const char *text, u32 x, u32 y, u32 color);
void (*window_resize)(Window *window, s32 new_width, s32 new_height);

static Window *doomfixer_window;
static struct task *doomfixer_task;

static struct fb_info* getfb_hook()
{
    struct fb_info* ret = alloc(sizeof(struct fb_info));

    ret->fb_addr = (i_ptr)doomfixer_window->buffer+doomfixer_window->width*20;
    ret->shadow_fb_addr = ret->fb_addr;
    ret->screen_width = doomfixer_window->width;
    ret->screen_height = doomfixer_window->height - 20;
    ret->screen_pitch = doomfixer_window->width*4;
    ret->bpp = 4;

    ret->font_height = 16;

    return ret;
}

static void doomfixer_thread() {
    APICALL("_elfload",'A',"./doom.elf");
    while(true) { yield(); }
}




INIT void init(void* _resolve_function(char* name))
{
    api_init(_resolve_function);
    window_create = _resolve_function("_window_create");
    window_render = _resolve_function("_window_render");
    window_fill_rect = _resolve_function("_window_fill_rect");
    draw_string = _resolve_function("_draw_string");
    draw_char_bg = _resolve_function("_draw_char_bg");
    compositor_present = _resolve_function("_compositor_present");
    window_is_active = _resolve_function("_window_is_active");
    desktop_fill_rect = _resolve_function("_desktop_fill_rect");
    desktop_draw_string = _resolve_function("_desktop_draw_string");
    window_resize = _resolve_function("_window_resize");

    doomfixer_window = window_create("doom");
    window_resize(doomfixer_window,640,480);
    doomfixer_window->window_on_changed = 1;
    window_render(doomfixer_window);
    replace_function("_getfb",getfb_hook);
    doomfixer_window->task = create_task("doomloader", doomfixer_thread);
}
