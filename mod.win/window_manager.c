/*
 * Copyright (c) 2026 ilizavr & TINERKOTL & yellowhat
 * SPDX-License-Identifier: MIT
 */

/*
 * Э то основн*ой модуль Windows Manager.
 *
 * Если у вас есть замечания, идеи как можно оптимизировать или какие-нибудь новые функции.
 * Пишите мне в ТГ (ссылка в описании на github) или в самом github в кладке "issues".
 *
 * Если вы пишите приложение, добавляйте их в свой репозиторий и пишите мне, чтобы я добавил вас, как создателя того или иного приложения.
 * В будущем будет создан сайт, где будут добавлены все приложения и их создатели.
 *
 * Удачи в написании приложений, жду ваши работы.
 */


#include "WM_types.h"

u32 (*getchar)();
void *(*alloc)(u32 size);
void (*free)(void *ptr);
void (*printf)(char* fmt, ...);
bool *(*get_key_state_matrix)();
s32 (*get_mouse_x)();
s32 (*get_mouse_y)();
bool (*get_mouse_left)();
bool (*get_mouse_right)();
void (*register_function)(char *function_name, void* call, char *description);
void (*print_color)(char*,u32);
struct file* (*open)(char diskletter, char* path);
void (*sleep)(u32 ms);
u32 (*getticks)();
void (*yield)();
bool (*kill)(u32 tid);
static bool (*unload_mod)(u32 idx);

struct task* (*create_task)(char *name, void* function);

struct task* task;

struct fb_info* (*fbcon_stop)();

struct fb_info* fb;

static DirtyRect dirty = {0,0,0,0, false};

#define MAX_WINDOWS 8
#define MAX_WINDOW_WIDTH 800
#define MAX_WINDOW_HEIGHT 600
#define MAX_DESKTOP_ICONS 16

bool changed = false;

u8 *second_buffer = 0;
u32 second_buffer_size = 0;
u32 mouse_x = 0, mouse_y = 0;
s32 prev_mouse_x = -1, prev_mouse_y = -1;

Window windows[MAX_WINDOWS];
Window *z_order[MAX_WINDOWS];
u32 window_count = 0;
u32 active_index = 0;
u32 fps_frames = 0;
u32 fps_last_tick = 0;
u32 fps_value = 0;
Window *active_window = 0;
s32 first_icon_x = 24, first_icon_y = 24, icon_size = 40;
DesktopIcon desktop_icons[MAX_DESKTOP_ICONS];
u32 desktop_icon_count = 0;

static s32 SW, SH, PP;
static u32 *sb;
static u32 *vram;

// static bool desktop_register_icon(u32 x, u32 y, u32 size, DesktopIconDraw draw) {
//     if (!draw || desktop_icon_count >= MAX_DESKTOP_ICONS)
//         return false;

//     desktop_icons[desktop_icon_count++] = (DesktopIcon){x, y, size, draw};
//     return true;
// }

static inline void cpy32(void *d, const void *s, u32 n) {
    asm volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(n) : : "memory");
}

static inline void set32(void *d, u32 v, u32 n) {
    asm volatile("rep stosl" : "+D"(d), "+c"(n) : "a"(v) : "memory");
}

static s32 dirty_min(s32 a, s32 b) {
    return a < b ? a : b;
}

static s32 dirty_max(s32 a, s32 b) {
    return a > b ? a : b;
}

static void dirty_rect_union(s32 x, s32 y, s32 w, s32 h) {
    if (w <= 0 || h <= 0)
        return;

    if (!dirty.valid) {
        dirty.x = x;
        dirty.y = y;
        dirty.w = w;
        dirty.h = h;
        dirty.valid = true;
        return;
    }

    s32 x0 = dirty_min(dirty.x, x);
    s32 y0 = dirty_min(dirty.y, y);
    s32 x1 = dirty_max(dirty.x + dirty.w, x + w);
    s32 y1 = dirty_max(dirty.y + dirty.h, y + h);

    dirty.x = x0;
    dirty.y = y0;
    dirty.w = x1 - x0;
    dirty.h = y1 - y0;
    dirty.valid = true;
}

static const u32 wallpaper_16x16[] = {
    0xFF000000, 0xFF111111, 0xFF222222, 0xFF333333,
    0xFF111111, 0xFF222222, 0xFF333333, 0xFF444444,
    0xFF222222, 0xFF333333, 0xFF444444, 0xFF555555,
    0xFF333333, 0xFF444444, 0xFF555555, 0xFF666666
};

static u32 desktop_palette[] = {
    0xFF1A1A1A,  // тёмно-серый
    0xFF2E3A59,  // синий
    0xFF234E3A,  // зелёный
    0xFF4A2D2D,  // тёплый красный
    0xFF3A2F50   // фиолетовый
};

static u32 desktop_bg_color = 0xFF1A1A1A;
static u32 *desktop_bitmap = 0;

static u32 desktop_pattern_color(u32 x, u32 y) {
    u32 grid = 24;

    if ((x % grid) < 2 || (y % grid) < 2)
        return 0xFF4A2D2D;

    return 0xFF1A1A1A;
}

u32 *desktop_background_buffer = 0;

static void build_desktop_background() {
    if (!desktop_background_buffer) {
        desktop_background_buffer = alloc(second_buffer_size);
        if (!desktop_background_buffer)
            return;
    }

    const u32 grid = 24;
    const u32 line = 0xFF4A2D2D;
    const u32 fill = 0xFF1A1A1A;

    for (u32 y = 0; y < SH; y++) {
        u32 *row = desktop_background_buffer + y * PP;

        if (y % grid < 2) {
            set32(row, line,SW);
            continue;
        }

        set32(row, fill, SW);

        for (u32 x = 0; x < SW; x += grid) {
            u32 n = (x + 2 <= SW) ? 2 : 1;
            set32(row + x, line, n);
        }
    }
}

static u32 arrow_cursor[11][6] = {
    {1, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0},
    {1, 1, 1, 0, 0, 0},
    {1, 1, 1, 1, 0, 0},
    {1, 1, 1, 1, 1, 0},
    {1, 1, 1, 1, 0, 0},
    {1, 1, 1, 1, 0, 0},
    {1, 1, 1, 1, 0, 0},
    {1, 0, 1, 1, 1, 0},
    {0, 0, 0, 1, 1, 0},
    {0, 0, 0, 0, 1, 1}
};

static u32 arrow_cursor_height[11][6] = {
    {0, 0, 1, 1, 0, 0},
    {0, 1, 1, 1, 1, 0},
    {1, 1, 1, 1, 1, 1},
    {1, 0, 1, 1, 0, 1},
    {0, 0, 1, 1, 0, 0},
    {0, 0, 1, 1, 0, 0},
    {0, 0, 1, 1, 0, 0},
    {1, 0, 1, 1, 0, 1},
    {1, 1, 1, 1, 1, 1},
    {0, 1, 1, 1, 1, 0},
    {0, 0, 1, 1, 0, 0}
};

static u32 arrow_cursor_width[11][6] = {
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0},
    {0, 1, 0, 0, 1, 0},
    {1, 1, 0, 0, 1, 1},
    {1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1},
    {1, 1, 0, 0, 1, 1},
    {0, 1, 0, 0, 1, 0},
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0}
};

static const u32 window_icon_1[5][5] = {
    {1, 0, 0, 0, 1},
    {0, 1, 0, 1, 0},
    {0, 0, 1, 0, 0},
    {0, 1, 0, 1, 0},
    {1, 0, 0, 0, 1},
};

static const u32 window_icon_2[5][5] = {
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1},
};

static const u32 window_icon_3[10][8] = {
    {0, 0, 1, 1, 1, 1, 1, 1},
    {0, 0, 1, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 0, 0, 1},
    {1, 0, 1, 0, 1, 0, 0, 1},
    {1, 0, 1, 0, 1, 0, 0, 1},
    {1, 0, 1, 0, 1, 0, 0, 1},
    {1, 0, 1, 0, 1, 0, 0, 1},
    {1, 1, 1, 1, 1, 0, 0, 1},
    {0, 0, 1, 0, 0, 0, 0, 1},
    {0, 0, 1, 1, 1, 1, 1, 1},
};

u32 (*mouse_now)[6] = arrow_cursor;

static void fps_on_present(void) {
    fps_frames++;

    u32 now = getticks();
    u32 elapsed = now - fps_last_tick;

    if (elapsed >= 1000) {
        fps_value = fps_frames * 1000 / elapsed;
        fps_frames = 0;
        fps_last_tick = now;
    }
}

static Window *free_window_slot() {
    for (u32 i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].buffer == 0)
            return &windows[i];
    }

    return 0;
}

static s32 win_rows(Window *window) {
    if (!window || !window->buffer || window->width <= 0)
        return 0;

    s32 r = (s32)(window->buffer_size / (u32)window->width);
    return r < window->height ? r : window->height;
}

static Window *window_create(char *name) {
    if (window_count >= MAX_WINDOWS)
        return 0;

    Window *window = free_window_slot();

    if (!window)
        return 0;

    memset(window, 0, sizeof(*window));

    window->x = 150;
    window->y = 200;
    window->width = 400;
    window->height = 300;

    window->invisible = 0;
    window->name = name;
    window->full_screen = false;

    window->buffer_size = window->width*window->height;

    window->buffer = alloc(window->buffer_size * sizeof(u32));
    if(!window->buffer)
        return 0;

    set32(window->buffer, 0x00000000, window->buffer_size);

    z_order[window_count] = window;

    window_count++;

    active_window = window;

    dirty_rect_union(window->x, window->y, window->width, window->height);
    changed = true;

    return window;
}

static void restore_rect_from_background(s32 x, s32 y, s32 w, s32 h) {
    if (w <= 0 || h <= 0)
        return;

    if (x < 0) {
        w += x;
        x = 0;
    }

    if (y < 0) {
        h += y;
        y = 0;
    }

    if (x + w > (s32)SW)
        w = (s32)SW - x;

    if (y + h > (s32)SH)
        h = (s32)SH - y;

    if (w <= 0 || h <= 0)
        return;

    for (s32 row = 0; row < h; row++) {
        cpy32(sb + (u32)(y + row) * PP + (u32)x, desktop_background_buffer + (u32)(y + row) * PP + (u32)x, (u32)w);
    }

    dirty_rect_union(x, y, w, h);
}

static void window_restore_old_area(Window *window) {
    if (!window)
        return;

    restore_rect_from_background(window->x, window->y, window->width, window->height);
}

static void present_framebuffer() {
    if (!dirty.valid)
        return;

    s32 x = dirty.x;
    s32 y = dirty.y;
    s32 w = dirty.w;
    s32 h = dirty.h;

    if (x < 0) {
        w += x;
        x = 0;
    }

    if (y < 0) {
        h += y;
        y = 0;
    }

    if (x + w > (s32)SW)
        w = (s32)SW - x;

    if (y + h > (s32)SH)
        h = (s32)SH - y;

    if (w <= 0 || h <= 0) {
        dirty.valid = false;
        return;
    }

    u32 *src = sb + (u32)y * PP + (u32)x;
    u32 *dst = vram + (u32)y * PP + (u32)x;

    for (s32 row = 0; row < h; row++) {
        cpy32(dst + (u32)row * PP, src + (u32)row * PP, (u32)w);
    }

    dirty.valid = false;

    // fps_on_present();
}

static void clearframe_win(Window *window, u32 color) {
    if (!window || !window->buffer)
        return;

    set32(window->buffer, color, window->buffer_size);
}

static void clearframe() {
    cpy32(sb, desktop_background_buffer, SH*PP);
}

static void put_pixel(u32 x, u32 y, u32 color) {
    if (x >= SW || y >= SH) return;

    sb[y * PP + x] = color | 0xFF000000;
}

static void put_pixel_window(Window *window, u32 x, u32 y, u32 color) {
    if (x >= window->width || y >= window->height)
        return;

    window->buffer[y * window->width + x] = color;
}

static void draw_char(Window *window, u8 code, u32 x, u32 y, u32 color) {
    for (u32 row = 0; row < 16; row++) {
        for (u32 col = 0; col < 8; col++) {
            if (font8x16[16*code + row] & (0x80 >> col))
                put_pixel_window(window, x + col, y + row, color);
        }
    }
}

static void draw_char_bg(Window *window, u8 code, u32 x, u32 y, u32 color, u32 bgcolor) {
    for (u32 row = 0; row < 16; row++) {
        for (u32 col = 0; col < 8; col++) {
            if (font8x16[16*code + row] & (0x80 >> col))
                put_pixel_window(window, x + col, y + row, color);
            else
                put_pixel_window(window, x + col, y + row, bgcolor);
        }
    }
}

static void draw_string(Window *window, const char *str, u32 x, u32 y, u32 color)
{
    while (*str) {
        draw_char(window, (u8)*str, x, y, color);
        x += 8;
        str++;
    }
}

static void draw_border_window(Window *window, u32 color) {
    s32 wx = window->x;
    s32 wy = window->y;
    s32 ww = window->width;
    s32 wh = window->height;

    if (ww <= 0 || wh <= 0)
        return;

    s32 x0 = wx < 0 ? 0 : wx;
    s32 x1 = wx + ww > (s32)SW ? (s32)SW : wx + ww;

    if (x0 < x1) {
        if (wy >= 0 && wy < (s32)SH)
            set32(sb + (u32)wy*PP + (u32)x0, color, (u32)(x1 - x0));
        if (wy + wh - 1 >= 0 && wy + wh - 1 <(s32)SH)
            set32(sb + (u32)(wy + wh - 1) * PP + (u32)x0, color, (u32)(x1 - x0));
    }

    s32 y0 = wy < 0 ? 0 : wy;
    s32 y1 = wy + wh > (s32)SH ? (s32)SH : wy + wh;
    s32 xl = wx;
    s32 xr = wx + ww - 1;

    for (s32 yy = y0; yy < y1; yy++){
        u32 *row = sb + (u32)yy * PP;
        if (xl >= 0 && xl < (s32)SW)
            row[xl] = color;
        if (xr >= 0 && xr < (s32)SW && xr != xl)
            row[xr] = color;
    }
}

static void window_fill_rect(Window *window, u32 x, u32 y, u32 width, u32 height, u32 color)
{
    if (!window || !window->buffer || window->width <= 0)
        return;

    s32 rows = win_rows(window);
    if (rows > window->height)
        rows = window->height;
    if (rows <= 0)
        return;

    s32 sx = (s32)x, sy = (s32)y;
    s32 sw = (s32)width, sh = (s32)height;

    if (sx < 0) { sw += sx; sx = 0; }
    if (sy < 0) { sh += sy; sy = 0; }
    if (sx + sw > window->width) sw = window->width - sx;
    if (sy + sh > rows)          sh = rows - sy;

    if (sw <= 0 || sh <= 0)
        return;

    u32 w32 = (u32)window->width;
    u32 *first_row = window->buffer + (u32)sy * w32 + (u32)sx;

    set32(first_row, color, (u32)sw);

    for (s32 row = 1; row < sh; row++) {
        cpy32(window->buffer + (u32)(sy + row) * w32 + (u32)sx, first_row, (u32)sw);
    }
}

static void present_window(Window *window) {
    s32 src_x = 0;
    s32 src_y = 0;
    s32 dst_x = window->x;
    s32 dst_y = window->y;

    s32 copy_width = window->width;
    s32 copy_height = window->height;

    if (window->full_screen) {
        s32 h = copy_height < (s32)SH ? copy_height : (s32)SH;
        u32 n = copy_width < (s32)PP ? copy_width : (s32)PP;

        for (s32 row = 0; row < h; row++) {
            cpy32(sb + (u32)row * PP, window->buffer + (u32)row * (u32)window->width, (u32)n);
        }

        dirty_rect_union(0, 0, SW, SH);
        return;
    }

    if (dst_x < 0) {
        src_x = -dst_x;
        copy_width -= src_x;
        dst_x = 0;
    }

    if (dst_y < 0) {
        src_y = -dst_y;
        copy_height -= src_y;
        dst_y = 0;
    }

    if (dst_x + copy_width > (s32)SW)
        copy_width = SW - dst_x;

    if (dst_y + copy_height > (s32)SH)
        copy_height = SH - dst_y;

    if (copy_width <= 0 || copy_height <= 0)
        return;

    dirty_rect_union(dst_x, dst_y, copy_width, copy_height);

    u32 *dst = sb + (u32)dst_y * (u32)PP + (u32)dst_x;
    u32 *src = window->buffer + (u32)src_y * (u32)window->width + (u32)src_x;

    for (s32 y = 0; y < copy_height; y++) {
        cpy32(dst, src, copy_width);
        dst += PP;
        src += (u32)window->width;
    }
}


#define CURSOR_WIDTH 6
#define CURSOR_HEIGHT 11

static u32 mouse_save[CURSOR_HEIGHT * CURSOR_WIDTH];
static s32 ms_x = -1, ms_y = 0, ms_w = 0, ms_h = 0;

static void mouse_snapshot() {
    s32 cx = (s32)mouse_x;
    s32 cy = (s32)mouse_y;
    s32 x0 = cx;
    s32 y0 = cy;
    s32 x1 = cx + CURSOR_WIDTH;
    s32 y1 = cy + CURSOR_HEIGHT;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (s32)SW) x1 = (s32)SW;
    if (y1 > (s32)SH) y1 = (s32)SH;

    if (x0 >= x1 || y0 >= y1){
        ms_x = -1;
        return;
    }

    for (s32 r = 0; r < y1 - y0; r++)
        cpy32(mouse_save + (u32)r * CURSOR_WIDTH, sb + (u32)(y0 + r) * PP + (u32)x0, (u32)(x1 - x0));

    ms_x = x0;
    ms_y = y0;
    ms_w = x1 - x0;
    ms_h = y1 - y0;
}

static void mouse_restore() {
    if (ms_x < 0)
        return;

    for (s32 r = 0; r < ms_h; r++)
        cpy32(sb + (u32)(ms_y + r) * PP + (u32)ms_x, mouse_save + (u32)r * CURSOR_WIDTH, (u32)ms_w);

    dirty_rect_union(ms_x, ms_y, ms_w, ms_h);
    ms_x = -1;
}

static void mouse_render() {
    s32 cx = (s32)mouse_x;
    s32 cy = (s32)mouse_y;
    s32 x0 = cx < 0 ? 0 : cx;
    s32 y0 = cy < 0 ? 0 : cy;
    s32 x1 = cx + CURSOR_WIDTH > (s32)SW ? (s32)SW : cx + CURSOR_WIDTH;
    s32 y1 = cy + CURSOR_HEIGHT > (s32)SH ? (s32)SH : cy + CURSOR_HEIGHT;

    for (s32 r = y0; r < y1; r++) {
        for (s32 c = x0; c < x1; c++) {
            u32 gx = (u32)(c - cx);
            u32 gy = (u32)(r - cy);
            if (mouse_now[gy][gx]) {
                sb[(u32)r * PP + (u32)c] = 0xFFFF0000;
            }
        }
    }

    if (x0 < x1 && y0 < y1)
        dirty_rect_union(x0, y0, x1 - x0, y1 - y0);
}

static void mouse_redraw_after_scene() {
    ms_x = -1;
    mouse_snapshot();
    mouse_render();
}

static void mouse_redraw_moved() {
    mouse_restore();
    mouse_snapshot();
    mouse_render();
}

static void draw_icons_window(Window *window) {
    s32 icon_pos = window->width - 70;

    for (u32 dy = 0; dy < 5; dy++) {
        for (u32 dx = 0; dx < 5; dx++) {
            if (!window_icon_1[dy][dx])
                continue;

            u32 px = icon_pos + 45 + dx;
            u32 py = 8 + dy;

            put_pixel_window(window, px, py, 0x00000000);
        }
    }

    for (u32 dy = 0; dy < 5; dy++) {
        for (u32 dx = 0; dx < 5; dx++) {
            if (!window_icon_2[dy][dx])
                continue;

            u32 px = icon_pos + dx;
            u32 py = 8 + dy;

            put_pixel_window(window, px, py, 0x00000000);
        }
    }

    for (u32 dy = 0; dy < 10; dy++) {
        for (u32 dx = 0; dx < 8; dx++) {
            if (!window_icon_3[dy][dx])
                continue;

            u32 px = icon_pos + 20 + dx;
            u32 py = 5 + dy;

            put_pixel_window(window, px, py, 0x00000000);
        }
    }
}

static void draw_window_content(Window *window) {
    window_fill_rect(window, 0, 0, window->width, 20, 0xFFFFFFFF);
    draw_string(window, window->name, 5, 2, 0x000000);
    draw_icons_window(window);
}

// void desktop_fill_rect(s32 x, s32 y, s32 width, s32 height, u32 color) {
//     for (u32 row = 0; row < height; row++) {
//         for (u32 col = 0; col < width; col++) {
//             u32 px = x + col;
//             u32 py = y + row;

//             if (px >= 0 && py >= 0)
//                 put_pixel(px, py, color);
//         }
//     }
// }

// void desktop_draw_string(const char *text, u32 x, u32 y, u32 color) {
//     while (*text) {
//         u8 code = (u8)*text++;

//         for (u32 row = 0; row < 16; row++) {
//             u8 bits = font8x16[16 * code + row];

//             for (u32 col = 0; col < 8; col++) {
//                 if (bits & (0x80 >> col))
//                     put_pixel(x + col, y + row, color);
//                 }
//             }

//             x += 8;
//     }
// }

static void draw_desktop() {
    for (u32 i = 0; i < desktop_icon_count; i++){
        DesktopIcon *icon = &desktop_icons[i];
        printf("before icon draw\n");
        icon->draw(icon->x, icon->y, icon->size);
        printf("after icon draw\n");
    }
}

static void wm_render(void) {
    if (!active_window || !active_window->full_screen){
        clearframe();
    }

    for (u32 i = 0; i < window_count; i++){
        Window *window = z_order[i];
        if (!window || window->invisible)
            continue;
        draw_window_content(window);
        present_window(window);
        draw_border_window(window, 0x00FFFFFF);
    }
}

static void compositor_present() {
    changed = true;
}

static void window_raise(Window *window) {
    if(window == 0)
        return;

    u32 index = 0;

    while (index < window_count) {
        if (z_order[index] == window)
            break;
        index++;
    }

    if (index >= window_count)
        return;

    for (u32 i = index; i + 1 < window_count; i++){
        z_order[i] = z_order[i+1];
    }

    z_order[window_count-1] = window;
}

static void window_resize(Window *window, s32 new_width, s32 new_height) {
    if (!window)
        return;

    s32 old_x = window->x;
    s32 old_y = window->y;
    s32 old_w = window->width;
    s32 old_h = window->height;

    u32 nw = (u32)new_width;
    u32 nh = (u32)new_height;

    if (nw == 0 || nh == 0)
        return;

    if (!window->full_screen) {
        if (nw > MAX_WINDOW_WIDTH)
            nw = MAX_WINDOW_WIDTH;
        if (nh > MAX_WINDOW_HEIGHT)
            nh = MAX_WINDOW_HEIGHT;
    }

    if (nw == (u32)window->width && nh == (u32)window->height)
        return;

    dirty_rect_union(old_x, old_y, old_w, old_h);

    u32 *new_buffer = alloc(nw * nh * sizeof(u32));
    if (!new_buffer)
        return;

    set32(new_buffer, 0xFF000000, nw * nh);

    u32 copy_w = nw < (u32)window->width ? nw : (u32)window->width;
    u32 copy_h = nh < (u32)window->height ? nh : (u32)window->height;

    for (u32 y = 0; y < copy_h; y++) {
        memcpy(&new_buffer[y * nw], &window->buffer[y * window->width], copy_w * sizeof(u32));
    }

    free(window->buffer);

    window->buffer = new_buffer;
    window->buffer_size = nw * nh;
    window->width = (s32)nw;
    window->height = (s32)nh;

    dirty_rect_union(window->x, window->y, window->width, window->height);

    if (window->on_resize)
        window->on_resize(window, old_w, old_h);
}

static void window_switch_next()
{
    if (window_count == 0)
        return;

    if (active_window == 0) {
        active_window = z_order[window_count - 1];
        active_index = window_count - 1;
        return;
    }

    u32 index = 0;

    while (index < window_count && z_order[index] != active_window)
        index++;

    if (index >= window_count)
        index = 0;
    else
        index = (index + 1) % window_count;

    active_window = z_order[index];

    window_raise(active_window);

    if (active_window)
        dirty_rect_union(active_window->x, active_window->y, active_window->width, active_window->height);

    changed = true;
}

static void resize_active_window(s32 dw, s32 dh)
{
    if (active_window == 0)
        return;

    s32 width = active_window->width + dw;
    s32 height = active_window->height + dh;

    if (width < 20)
        width = 20;

    if (height < 20)
        height = 20;

    if (width > MAX_WINDOW_WIDTH)
        width = MAX_WINDOW_WIDTH;

    if (height > MAX_WINDOW_HEIGHT)
        height = MAX_WINDOW_HEIGHT;

    window_resize(active_window, width, height);
    changed = true;
}

static void drawimage(Window *window, struct image*img,int startx,int starty)
{
    for(int x = 0;x<img->width;x++)
        for(int y = 0;y<img->height;y++)
            put_pixel_window(window, x+startx, y+starty, img->bytes[y*img->width+x]);
}

static void window_render(Window *window) {
    clearframe_win(window, 0xFFAAAAAA);

    window_fill_rect(window, 0, 0, window->width, 20, 0xFFFFFFFF);

    draw_string(window, window->name, 5, 2, 0x000000);

    draw_icons_window(window);

    changed = true;
}

static u32 clamp(s32 value, s32 min, s32 max)
{
    if (value < min)
        return min;

    if (value > max)
        return max;

    return value;
}

static void window_full_screen(Window *window, bool full_screen) {
    if (full_screen) {
        window->prev_x = window->x;
        window->prev_y = window->y;
        window->prev_width = window->width;
        window->prev_height = window->height;

        window->full_screen = true;
        window->x = 0;
        window->y = 0;

        dirty_rect_union(window->prev_x, window->prev_y, window->prev_width, window->prev_height);

        window_resize(window, SW, SH);

        dirty_rect_union(0, 0, SW, SH);
    } else {
        dirty_rect_union(window->x, window->y, window->width, window->height);
        window->full_screen = false;
        window->x = window->prev_x;
        window->y = window->prev_y;

        window_resize(window, window->prev_width,  window->prev_height);
        dirty_rect_union(window->x, window->y, window->width, window->height);
    }
    changed = true;
}

static void window_destroy(Window *window) {
    if (!window)
        return;

    window_restore_old_area(window);

    u32 index = 0;

    while (index < window_count && z_order[index] != window)
        index++;

    if (index >= window_count)
        return;

    for (u32 i = index; i + 1 < window_count; i++)
        z_order[i] = z_order[i+1];

    window_count--;

    if(active_window == window) {
        active_window = 0;

        for (u32 i = 0;i < window_count; i++) {
            if (z_order[i] && !z_order[i]->invisible) {
                active_window = z_order[i];
                break;
            }
        }
    }

    if (window->task) {
        int module_idx = window->task->module_idx;
        kill(window->task->tid);
        window->task = 0;
        free(window->buffer);
        window->buffer = 0;
        window->buffer_size = 0;

        unload_mod((u32)module_idx);
    }

    changed = true;
}

static bool window_is_active(Window *window) {
    return window && window == active_window && !window->invisible;
}

static void init_WM() {

    u8 wcode = ascii_to_keycode['w'];
    u8 dcode = ascii_to_keycode['d'];
    u8 scode = ascii_to_keycode['s'];
    u8 acode = ascii_to_keycode['a'];
    u8 pluscode = ascii_to_keycode['+'];
    u8 mincode = ascii_to_keycode['-'];
    u8 onecode = ascii_to_keycode['1'];

    bool *key_state_matrix = get_key_state_matrix();

    bool old_window = false;

    fb = fbcon_stop();

    SW = fb->screen_width;
    SH = fb->screen_height;
    PP = fb->screen_pitch / 4;
    vram = (u32 *)fb->fb_addr;

    second_buffer_size = SH * fb->screen_pitch;
    second_buffer = alloc(second_buffer_size);

    if (!second_buffer)
        return;

    memset(second_buffer, 0, second_buffer_size);

    sb = (u32*)second_buffer;
    desktop_background_buffer = 0;
    memset(windows, 0, sizeof(windows));
    window_count = 0; active_window = 0;
    prev_mouse_x = prev_mouse_y = -1;
    changed = false;

    build_desktop_background();
    wm_render();
    dirty_rect_union(0,0,SW,SH);
    mouse_redraw_after_scene();
    present_framebuffer();

    s32 old_mouse_x = get_mouse_x();
    s32 old_mouse_y = get_mouse_y();

    s32 old_left = 0;
    s32 old_right = 0;

    u32 drag_offsets_x = 0;
    u32 drag_offsets_y = 0;
    s32 old_width = 0, old_height = 0;
    s32 resize_start_x = 0, resize_start_y = 0;
    s32 old_x = 0, old_y = 0;
    s32 last_frame = 0;

    bool dragging = false;
    bool size_changed = false;
    bool on_left = false, on_right = false, on_top = false, on_down = false;

    fps_last_tick = getticks();

    while(1){
        mouse_now = arrow_cursor;

        mouse_x = get_mouse_x();
        mouse_y = get_mouse_y();

        s32 left = get_mouse_left();
        s32 right = get_mouse_right();

        bool left_pressed = left && !old_left;
        bool left_hold = left != 0;
        bool left_released = !left && old_left;

        bool right_pressed = right && !old_right;
        bool right_released = !right && old_right;

        if (active_window && active_window->invisible) {
            active_window = 0;
            for (u32 i = 0; i < window_count; i++) {
                if (z_order[i] && !z_order[i]->invisible) {
                    active_window = z_order[i];
                    break;
                }
            }
        }

        if (left_pressed) {
            Window *hit = 0;
            for (u32 i = window_count; i > 0; i--){
                Window *window = z_order[i - 1];

                if (window->invisible)
                    continue;

                if (mouse_x >= window->x && mouse_x < window->x + window->width && mouse_y >= window->y && mouse_y < window->y + window->height) {
                    hit = window;
                    break;
                }
            }

            dragging = false;
            size_changed = false;

            if (hit) {
                active_window = hit;
                window_raise(hit);
                dirty_rect_union(hit->x, hit->y, hit->width, hit->height);

                drag_offsets_x = mouse_x - hit->x;
                drag_offsets_y = mouse_y - hit->y;

                bool in_tittle = drag_offsets_y < 20;
                bool tittle_button_clicked = false;

                if (in_tittle) {
                    s32 Collapse = hit->width - 70;
                    s32 full_screen = hit->width - 50;
                    s32 close  = hit->width - 25;

                    if ((drag_offsets_x - Collapse) * (drag_offsets_x - Collapse) + (drag_offsets_y - 10) * (drag_offsets_y - 10) <= 49) {
                        active_window->invisible = 1;
                        tittle_button_clicked = true;
                        dirty_rect_union(0, 0, SW, SH);
                        changed = true;
                    }
                    if ((drag_offsets_x - full_screen) * (drag_offsets_x - full_screen) + (drag_offsets_y - 10) * (drag_offsets_y - 10) <= 49) {
                        hit->full_screen = !hit->full_screen;
                        window_full_screen(hit, hit->full_screen);
                        tittle_button_clicked = true;
                    }

                    if ((drag_offsets_x - close) * (drag_offsets_x - close) + (drag_offsets_y - 10) * (drag_offsets_y - 10) <= 49) {
                        window_destroy(hit);
                        tittle_button_clicked = true;
                    }

                    if (hit && !tittle_button_clicked) {
                        dragging = true;
                    }
                } else{
                    on_left = drag_offsets_x < 10;
                    on_right = drag_offsets_x >= hit->width - 10;
                    on_top = drag_offsets_y < 3;
                    on_down = drag_offsets_y >= hit->height - 10;

                    if (on_left || on_down || on_right || on_top) {
                        old_width = hit->width;
                        old_height = hit->height;
                        old_x = hit->x;
                        old_y = hit->y;
                        resize_start_x = mouse_x;
                        resize_start_y = mouse_y;
                        size_changed = true;
                    }
                }
            }
        }

        if (left_hold && active_window) {
            s32 new_x = mouse_x - drag_offsets_x;
            s32 new_y = mouse_y - drag_offsets_y;

            if (dragging) {
                s32 old_rect_x = active_window->x;
                s32 old_rect_y = active_window->y;
                s32 old_rect_w = active_window->width;
                s32 old_rect_h = active_window->height;

                new_x = clamp(new_x, 0, (s32)SW - active_window->width);
                new_y = clamp(new_y, 0, (s32)SH - active_window->height);

                if (active_window->x != new_x || active_window->y != new_y) {
                    dirty_rect_union(old_rect_x, old_rect_y, old_rect_w, old_rect_h);
                    active_window->x = new_x;
                    active_window->y = new_y;
                    dirty_rect_union(active_window->x, active_window->y, active_window->width, active_window->height);
                    changed = true;
                }
            }

            if (size_changed && !active_window->window_on_changed) {
                dirty_rect_union(active_window->x, active_window->y, active_window->width, active_window->height);
                s32 dx = mouse_x - resize_start_x;
                s32 dy = mouse_y - resize_start_y;
                s32 x = resize_start_x;
                s32 y = resize_start_y;
                s32 width = old_width;
                s32 height = old_height;

                if (on_left) {
                    width = old_width - dx;
                    x = resize_start_x + old_width - width;

                    active_window->x = x;
                    mouse_now = arrow_cursor_width;
                }

                if (on_right) {
                    width = old_width + dx;
                    mouse_now = arrow_cursor_width;
                }

                if(on_down) {
                    height = old_height + dy;
                    mouse_now = arrow_cursor_height;
                }

                if (on_top) {
                    height = old_height - dy;
                    y = resize_start_y + old_height - height;
                    mouse_now = arrow_cursor_height;
                }

                if (on_down && on_left) {
                    width = old_width - dx;
                    x = resize_start_x + old_width - width;
                    height = old_height + dy;

                    active_window->x = x;
                }

                if (on_down && on_right) {
                    height = old_height + dy;
                    mouse_now = arrow_cursor_height;

                    width = old_width + dx;
                }

                window_resize(active_window, width, height);
                changed = true;
            }
        }

        if (left_released) {
            size_changed = false;
            dragging = false;
        }

        bool mouse_changed = mouse_x != old_mouse_x || mouse_y != old_mouse_y;
        u32 now = getticks();

        if ((changed || mouse_changed) && now != last_frame) {
            if (changed) {
                dirty_rect_union(ms_x, ms_y, ms_w, ms_h);
                wm_render();
                mouse_redraw_after_scene();
                changed = false;
            } else {
                mouse_redraw_moved();
            }

            present_framebuffer();
            old_mouse_x = mouse_x;
            old_mouse_y = mouse_y;
            prev_mouse_x = (s32)mouse_x;
            prev_mouse_y = (s32)mouse_y;
            last_frame = now;
        }

        old_left = left;
        old_right = right;

        yield();
    }
}

INIT void init(void* _resolve_function(char* name))
{
    printf = _resolve_function("_printf");
    getchar = _resolve_function("_getchar");
    fbcon_stop = _resolve_function("_fbcon_stop");
    open = _resolve_function("_open");
    alloc = _resolve_function("_alloc");
    free = _resolve_function("_free");
    get_mouse_x = _resolve_function("_get_mouse_x");
    get_mouse_y = _resolve_function("_get_mouse_y");
    get_mouse_left = _resolve_function("_get_mouse_left");
    get_mouse_right = _resolve_function("_get_mouse_right");
    register_function = _resolve_function("_register_function");
    print_color = _resolve_function("_print_color");
    sleep = _resolve_function("_sleep_ms");
    getticks = _resolve_function("_getticks");
    create_task = _resolve_function("_create_task");
    yield = _resolve_function("_yield");
    kill = _resolve_function("_kill");
    unload_mod = _resolve_function("_unload_mod");
    get_key_state_matrix = _resolve_function("_get_key_state_matrix");

    register_function("_window_create", window_create, "Create window");
    register_function("_window_render", window_render, "Render window");
    register_function("_clearframe_win", clearframe_win, "Clearframe windows");
    register_function("_window_fill_rect", window_fill_rect, "Draw window fill rect");
    register_function("_draw_string", draw_string, "Draw string");
    register_function("_draw_char", draw_char, "Draw char");
    register_function("_draw_char_bg", draw_char_bg, "Draw char with background");
    register_function("_draw_border_window", draw_border_window, "Draw border for window");
    register_function("_compositor_present", compositor_present, "Draw in main framebuffer");
    register_function("_window_is_active", window_is_active, "Check active window");
    register_function("_window_resize", window_resize, "Change size window");

    create_task("Windows_Manager", init_WM);
}
