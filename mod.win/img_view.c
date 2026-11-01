/*
 * Copyright (c) 2026 ilizavr & TINERKOTL & yellowhat
 * SPDX-License-Identifier: MIT
 */

#include "../lib/zhirapi.h"
#include "WM_types.h"

Window* (*window_create)(char *name);
void (*window_render)(Window *window);
void (*clearframe_win)(Window *window, u32 color);
void (*window_fill_rect)(Window *window, u32 x, u32 y, u32 width, u32 height, u32 color);
void (*draw_string)(Window *window, const char *str, u32 x, u32 y, u32 color);
void (*draw_char_bg)(Window *window, char chr, u32 x, u32 y, u32 color, u32 bgcolor);
void (*draw_border_window)(Window *window, u32 color);
void (*compositor_present)();
bool (*window_is_active)(Window *window);
void (*desktop_fill_rect)(s32 x, s32 y, s32 width, s32 height, u32 color);
void (*desktop_draw_string)(const char *text, u32 x, u32 y, u32 color);
void (*window_resize)(Window *window, s32 new_width, s32 new_height);

static struct task *img_task;

static void img_thread() {
    while(1) {
        compositor_present();
        yield();
    }
}

static void img_f(char diskletter, char* name)
{
    struct file *f = open(diskletter, name);
    if (!f) {
        return;
    }

    char * dupname = alloc(strlen(name)+1);
    strcpy(dupname, name);

    struct Window *img_window = window_create(dupname);
    img_window->task = 0;

    int size = f->getsize(f);
    struct image *img = alloc(size);
    f->read(f, img, size, 0);
    f->close(f);

    window_resize(img_window,img->width,img->height);
    img_window->window_on_changed = 1;

    window_render(img_window);

    u32 *win_buf = (u32*)img_window->buffer;
    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            win_buf[y * img->width + x] = img->bytes[y * img->width + x];
        }
    }

    free(img);
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

    if(_resolve_function("_img")) replace_function("_img", img_f);
    else register_function("_img", img_f, "_img;c:diskletter;name. view image");

    img_task = create_task("img", img_thread);
}
