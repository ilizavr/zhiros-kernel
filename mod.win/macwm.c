/*
 * Copyright (c) 2026 ilizavr & contributors
 * SPDX-License-Identifier: MIT
 */

// zhirmac: macOS-like window manager, square CLI look. Drop-in for win.mod (same _window_* API).

#include "../lib/zhirapi.h"
#include "WM_types.h"

#define BAR_H 22
#define TITLE_H 20
#define MAXW 16
#define NAPPS 3
#define TILE 36
#define GAP 8
#define PAD 6
#define DOCK_H 48
#define DOCK_RESERVE 56
#define MENU_W 208

#define C_BAR 0xFF0A0A0A
#define C_LINE 0xFF2E2E2E
#define C_TEXT 0xFFD4D4D4
#define C_DIM 0xFF6E6E6E
#define C_ACC 0xFF4ADE80
#define C_SEL 0xFF2563EB
#define C_PANEL 0xFF161616
#define C_FRAME 0xFF3C3C3C
#define C_FRAME_ACT 0xFF7A7A7A
#define C_WHITE 0xFFFFFFFF

static s32 (*mouse_xf)();
static s32 (*mouse_yf)();
static bool (*mouse_lf)();

static struct fb_info *fb;
static s32 SW, SH, PP;
static u32 *vram, *back, *bg, *tgt;

static Window *zo[MAXW];
static u32 wn, created;
static Window *active, *about_win, *hover_btn_win;
static s32 mx, my;
static int menu_open = -1;
static int hover_id;
static bool full, app_dirty, bar_dirty;

static Window *drag_w, *rs_w;
static s32 drag_dx, drag_dy, rs_mode, rs_mx, rs_my, rs_w0, rs_h0;

static s32 menu_x[3], menu_w[3];
static s32 dk_n, dk_x0, dk_y0, dk_w;
static s32 dk_x[NAPPS + MAXW];
static Window *dk_win[NAPPS + MAXW];

static char clock_str[32];
static char mem_str[24];

static s32 dx0, dy0, dx1, dy1;
static bool dv;

static const char cursor_shape[17][13] = {
    "#...........",
    "##..........",
    "#o#.........",
    "#oo#........",
    "#ooo#.......",
    "#oooo#......",
    "#ooooo#.....",
    "#oooooo#....",
    "#ooooooo#...",
    "#oooooooo#..",
    "#ooooo#####.",
    "#oo#oo#.....",
    "#o#.#oo#....",
    "##..#oo#....",
    "#....#oo#...",
    ".....#oo#...",
    "......##....",
};

/* ---------- low level ---------- */

static inline void cpy32(void *d, const void *s, u32 n) {
    asm volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(n) : : "memory");
}

static inline void set32(void *d, u32 v, u32 n) {
    asm volatile("rep stosl" : "+D"(d), "+c"(n) : "a"(v) : "memory");
}

static inline void io_out(u16 p, u8 v) { asm volatile("outb %0, %1" : : "a"(v), "Nd"(p)); }
static inline u8 io_in(u16 p) { u8 v; asm volatile("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }

static char *sput(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }

static char *nput(char *p, u32 v, int pad) {
    char t[12];
    int n = 0;
    do { t[n++] = '0' + v % 10; v /= 10; } while (v);
    while (n < pad) t[n++] = '0';
    while (n) *p++ = t[--n];
    *p = 0;
    return p;
}

/* ---------- screen drawing (into tgt) ---------- */

static bool clip(s32 *x, s32 *y, s32 *w, s32 *h) {
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > SW) *w = SW - *x;
    if (*y + *h > SH) *h = SH - *y;
    return *w > 0 && *h > 0;
}

static void sfill(s32 x, s32 y, s32 w, s32 h, u32 c) {
    if (!clip(&x, &y, &w, &h)) return;
    for (s32 r = 0; r < h; r++) set32(tgt + (y + r) * PP + x, c, w);
}

static void sframe(s32 x, s32 y, s32 w, s32 h, u32 c) {
    sfill(x, y, w, 1, c);
    sfill(x, y + h - 1, w, 1, c);
    sfill(x, y, 1, h, c);
    sfill(x + w - 1, y, 1, h, c);
}

static void sdither(s32 x, s32 y, s32 w, s32 h) {
    if (!clip(&x, &y, &w, &h)) return;
    for (s32 r = 0; r < h; r++)
        for (s32 c = ((x + y + r) & 1) ? 0 : 1; c < w; c += 2)
            tgt[(y + r) * PP + x + c] = 0xFF000000;
}

static void schar(s32 x, s32 y, u8 ch, u32 c) {
    for (s32 r = 0; r < 16; r++) {
        u8 bits = font8x16[16 * ch + r];
        s32 py = y + r;
        if (!bits || py < 0 || py >= SH) continue;
        for (s32 k = 0; k < 8; k++) {
            s32 px = x + k;
            if ((bits & (0x80 >> k)) && px >= 0 && px < SW) tgt[py * PP + px] = c;
        }
    }
}

static s32 sstr(s32 x, s32 y, const char *s, u32 c) {
    while (*s) { schar(x, y, (u8)*s++, c); x += 8; }
    return x;
}

static s32 sstrb(s32 x, s32 y, const char *s, u32 c) {
    sstr(x + 1, y, s, c);
    return sstr(x, y, s, c);
}

/* ---------- wallpaper ---------- */

static u32 rng_state = 0x2F6B1D3;

static u32 rnd(void) {
    rng_state = rng_state * 1103515245 + 12345;
    return rng_state >> 8;
}

static u32 lerp(u32 a, u32 b, s32 t, s32 n) {
    if (t < 0) t = 0;
    if (t > n) t = n;
    u32 r = 0xFF000000;
    for (int sh = 0; sh < 24; sh += 8) {
        s32 ca = (a >> sh) & 255, cb = (b >> sh) & 255;
        r |= (u32)(ca + (cb - ca) * t / n) << sh;
    }
    return r;
}

static void sline(s32 x0, s32 y0, s32 x1, s32 y1, u32 c) {
    s32 dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    s32 dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    s32 err = dx + dy;
    while (1) {
        if (x0 >= 0 && x0 < SW && y0 >= 0 && y0 < SH) tgt[y0 * PP + x0] = c;
        if (x0 == x1 && y0 == y1) break;
        s32 e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static s32 isqrt(s32 n) {
    s32 v = 0;
    while ((v + 1) * (v + 1) <= n) v++;
    return v;
}

static void mountains(s32 hz, s32 step, s32 hmax, s32 jitter, u32 top, u32 base) {
    s32 cx = SW / 2, h = hmax / 2;
    for (s32 x = 0; x < SW; x += step) {
        h += (s32)(rnd() % (2 * jitter + 1)) - jitter;
        s32 dist = x + step / 2 - cx;
        if (dist < 0) dist = -dist;
        s32 lim = 12 + hmax * dist / cx;
        if (h > lim) h = lim;
        if (h < 6) h = 6;
        for (s32 y = hz - h; y < hz; y++)
            sfill(x, y, step, 1, lerp(top, base, y - (hz - h), h));
    }
}

static void build_wallpaper(void) {
    s32 hz = SH * 62 / 100;
    s32 cx = SW / 2;

    for (s32 y = 0; y < hz; y++)
        sfill(0, y, SW, 1, lerp(0xFF060818, 0xFF3C1460, y, hz));

    for (int i = 0; i < 160; i++) {
        s32 x = rnd() % SW, y = BAR_H + rnd() % (hz * 2 / 3);
        if (rnd() % 6 == 0) sfill(x, y, 2, 2, 0xFFFFFFFF);
        else sfill(x, y, 1, 1, lerp(0xFF3A3A60, 0xFFC8D0FF, rnd() % 8, 7));
    }

    s32 r = SH / 5, cy = hz - r / 3;
    for (s32 y = cy - r; y < hz && y <= cy + r; y++) {
        s32 dy = y - cy;
        if (dy > -r / 4) {
            s32 k = dy + r / 4;
            if (k % 14 < k * 7 / (r + r / 4)) continue;
        }
        s32 hw = isqrt(r * r - dy * dy);
        sfill(cx - hw, y, 2 * hw, 1, lerp(0xFFFFE36E, 0xFFFF2F86, y - (cy - r), 2 * r));
    }

    mountains(hz, 16, SH / 4, 18, 0xFF3A1A5C, 0xFF1C0C30);
    mountains(hz, 8, SH / 7, 10, 0xFF1A0B2A, 0xFF0C0616);

    for (s32 y = hz; y < SH; y++)
        sfill(0, y, SW, 1, lerp(0xFF16082A, 0xFF040208, y - hz, SH - hz));

    for (s32 i = -24; i <= 24; i++)
        sline(cx + i * 10, hz, cx + i * SW / 7, SH - 1, 0xFF9A2A9E);

    for (s32 k = 1;; k++) {
        s32 y = hz + k * k * 2;
        if (y >= SH) break;
        sfill(0, y, SW, 1, lerp(0xFF5A1C70, 0xFFFF3DA0, y - hz, SH - hz));
    }

    sfill(0, hz, SW, 1, 0xFFFF5FB0);
    sfill(0, hz + 1, SW, 1, 0xFF8A2A80);
}

/* ---------- window buffer API ---------- */

static s32 win_rows(Window *w) {
    if (!w || !w->buffer || w->width <= 0) return 0;
    s32 r = (s32)(w->buffer_size / (u32)w->width);
    return r < w->height ? r : w->height;
}

static inline void wpx(Window *w, s32 rows, u32 x, u32 y, u32 c) {
    if (x >= (u32)w->width || y >= (u32)rows) return;
    w->buffer[y * w->width + x] = c;
}

static void api_draw_char(Window *w, u8 code, u32 x, u32 y, u32 c) {
    s32 rows = win_rows(w);
    if (!rows) return;
    for (u32 r = 0; r < 16; r++) {
        u8 bits = font8x16[16 * code + r];
        for (u32 k = 0; k < 8; k++)
            if (bits & (0x80 >> k)) wpx(w, rows, x + k, y + r, c);
    }
}

static void api_draw_char_bg(Window *w, u8 code, u32 x, u32 y, u32 c, u32 b) {
    s32 rows = win_rows(w);
    if (!rows) return;
    for (u32 r = 0; r < 16; r++) {
        u8 bits = font8x16[16 * code + r];
        for (u32 k = 0; k < 8; k++)
            wpx(w, rows, x + k, y + r, (bits & (0x80 >> k)) ? c : b);
    }
}

static void api_draw_string(Window *w, const char *s, u32 x, u32 y, u32 c) {
    while (*s) { api_draw_char(w, (u8)*s++, x, y, c); x += 8; }
}

static void api_fill(Window *w, u32 x, u32 y, u32 ww, u32 hh, u32 c) {
    s32 rows = win_rows(w);
    if (!rows || x >= (u32)w->width || y >= (u32)rows) return;
    if (x + ww > (u32)w->width) ww = w->width - x;
    if (y + hh > (u32)rows) hh = rows - y;
    for (u32 r = 0; r < hh; r++) set32(w->buffer + (y + r) * w->width + x, c, ww);
}

static void api_clear(Window *w, u32 c) {
    s32 rows = win_rows(w);
    if (rows) set32(w->buffer, c, rows * w->width);
}

static void api_border(Window *w, u32 c) {
    s32 rows = win_rows(w);
    if (rows < 2) return;
    api_fill(w, 0, 0, w->width, 1, c);
    api_fill(w, 0, rows - 1, w->width, 1, c);
    api_fill(w, 0, 0, 1, rows, c);
    api_fill(w, w->width - 1, 0, 1, rows, c);
}

/* ---------- window management ---------- */

static int zidx(Window *w) {
    for (u32 i = 0; i < wn; i++) if (zo[i] == w) return i;
    return -1;
}

static void zremove(Window *w) {
    int i = zidx(w);
    if (i < 0) return;
    for (u32 j = i; j + 1 < wn; j++) zo[j] = zo[j + 1];
    wn--;
}

static void zraise(Window *w) {
    if (zidx(w) < 0) return;
    zremove(w);
    zo[wn++] = w;
}

static void pick_active(void) {
    active = 0;
    for (int i = (int)wn - 1; i >= 0; i--)
        if (!zo[i]->invisible) { active = zo[i]; break; }
}

static void focus(Window *w) {
    w->invisible = 0;
    zraise(w);
    active = w;
    full = 1;
}

static Window *find_win(const char *name) {
    for (u32 i = 0; i < wn; i++) if (strcmp(zo[i]->name, name) == 0) return zo[i];
    return 0;
}

static Window *api_window_create(char *name) {
    if (wn >= MAXW) return 0;
    Window *w = alloc(sizeof(Window));
    if (!w) return 0;
    memset(w, 0, sizeof(Window));

    s32 ww = 400, hh = 300;
    if (name && strcmp(name, "terminal") == 0) { ww = 576; hh = 340; }

    u32 k = created++ % 6;
    w->x = 40 + 28 * k;
    w->y = BAR_H + 24 + 24 * k;
    w->width = ww;
    w->height = hh;
    w->name = name ? name : "untitled";
    w->buffer_size = ww * hh;
    w->buffer = alloc(w->buffer_size * 4);
    if (!w->buffer) { free(w); return 0; }
    set32(w->buffer, 0xFF000000, w->buffer_size);

    zo[wn++] = w;
    active = w;
    full = 1;
    return w;
}

static void draw_title(Window *w) {
    s32 rows = win_rows(w);
    if (rows < TITLE_H) return;
    bool act = w == active;

    api_fill(w, 0, 0, w->width, TITLE_H - 1, act ? 0xFF262626 : 0xFF1A1A1A);
    api_fill(w, 0, TITLE_H - 1, w->width, 1, 0xFF3A3A3A);

    api_fill(w, 8, 5, 10, 10, act ? 0xFFFF5F57 : 0xFF454545);
    api_fill(w, 24, 5, 10, 10, act ? 0xFFFEBC2E : 0xFF454545);
    api_fill(w, 40, 5, 10, 10, act ? 0xFF28C840 : 0xFF454545);

    if (hover_btn_win == w) {
        for (u32 t = 2; t < 8; t++) {
            wpx(w, rows, 8 + t, 5 + t, 0xFF4A0A0A);
            wpx(w, rows, 17 - t, 5 + t, 0xFF4A0A0A);
        }
        api_fill(w, 26, 9, 6, 2, 0xFF4A3000);
        api_fill(w, 42, 9, 6, 2, 0xFF0A3A0A);
        api_fill(w, 44, 7, 2, 6, 0xFF0A3A0A);
    }

    s32 tx = (w->width - (s32)strlen(w->name) * 8) / 2;
    if (tx < 60) tx = 60;
    api_draw_string(w, w->name, tx, 2, act ? 0xFFE6E6E6 : 0xFF7A7A7A);
}

static void api_window_render(Window *w) {
    api_clear(w, 0xFFAAAAAA);
    draw_title(w);
}

static void win_resize(Window *w, s32 nw, s32 nh) {
    if(w->window_on_changed) return;

    if (nw < 160) nw = 160;
    if (nh < 80) nh = 80;
    if (nw > SW) nw = SW;
    if (nh > SH) nh = SH;
    if (nw == w->width && nh == w->height) return;

    u32 *nb = alloc(nw * nh * 4);
    if (!nb) return;
    set32(nb, 0xFF000000, nw * nh);

    s32 rows = win_rows(w);
    s32 cw = nw < w->width ? nw : w->width;
    s32 ch = nh < rows ? nh : rows;
    for (s32 r = 0; r < ch; r++) cpy32(nb + r * nw, w->buffer + r * w->width, cw);

    s32 ow = w->width, oh = w->height;
    free(w->buffer);
    w->buffer = nb;
    w->buffer_size = nw * nh;
    w->width = nw;
    w->height = nh;
    if (w->on_resize) w->on_resize(w, ow, oh);
    full = 1;
}

static void win_zoom(Window *w) {
    if (!w->full_screen) {
        w->prev_x = w->x; w->prev_y = w->y;
        w->prev_width = w->width; w->prev_height = w->height;
        w->full_screen = 1;
        w->x = 1;
        w->y = BAR_H + 1;
        win_resize(w, SW - 2, SH - BAR_H - 1 - DOCK_RESERVE);
    } else {
        w->full_screen = 0;
        w->x = w->prev_x;
        w->y = w->prev_y;
        win_resize(w, w->prev_width, w->prev_height);
    }
    full = 1;
}

static void win_minimize(Window *w) {
    w->invisible = 1;
    if (active == w) pick_active();
    full = 1;
}

static void win_close(Window *w) {
    zremove(w);
    if (drag_w == w) drag_w = 0;
    if (rs_w == w) rs_w = 0;
    if (hover_btn_win == w) hover_btn_win = 0;

    if (w == about_win) {
        about_win = 0;
        free(w->buffer);
        free(w);
    } else if (w->task) {
        unload_mod(w->task->module_idx);
        kill(w->task->tid);
        w->task = 0;
        free(w->buffer);
        w->buffer = 0;
        w->buffer_size = 0;
    }
    // windows without a task keep their buffer: the app may still be drawing into it

    pick_active();
    full = 1;
}

static void api_compositor_present() { app_dirty = 1; }

static bool api_window_is_active(Window *w) {
    return w && w == active && !w->invisible && zidx(w) >= 0;
}

static void api_desk_fill(s32 x, s32 y, s32 w, s32 h, u32 c) {
    if (!bg) return;
    u32 *o = tgt; tgt = bg; sfill(x, y, w, h, c); tgt = o;
    full = 1;
}

static void api_desk_str(const char *s, u32 x, u32 y, u32 c) {
    if (!bg) return;
    u32 *o = tgt; tgt = bg; sstr(x, y, s, c); tgt = o;
    full = 1;
}

/* ---------- apps ---------- */

static const char *app_label(int a) {
    switch (a) { case 0: return "Terminal"; case 1: return "Pong Game"; default: return "doom"; }
}
static const char *app_wname(int a) {
    switch (a) { case 0: return "terminal"; case 1: return "Pong Game"; default: return "doom"; }
}
static const char *app_mod(int a) {
    switch (a) { case 0: return "./term.mod"; case 1: return "./pong.mod"; default: return "./doomloader.mod"; }
}

static void launch(int a) {
    Window *w = find_win(app_wname(a));
    if (w) { focus(w); return; }
    load_mod('A', (char *)app_mod(a));
    full = 1;
}

static void about_open(void) {
    if (about_win) { focus(about_win); return; }
    Window *w = api_window_create("About zhirOS");
    if (!w) return;
    w->width = 360;
    w->height = 200;
    w->x = (SW - 360) / 2;
    w->y = 140;
    api_clear(w, 0xFF101010);

    api_draw_string(w, "\xFE zhirOS", 25, 36, C_ACC);
    api_draw_string(w, "\xFE zhirOS", 24, 36, C_ACC);
    api_draw_string(w, "kernel  zhiros-kernel, x86 32-bit", 24, 66, C_TEXT);
    api_draw_string(w, "wm      zhirmac 0.1, square edition", 24, 86, C_TEXT);
    api_draw_string(w, "screen  800x600x32", 24, 106, C_TEXT);

    char line[40];
    char *p = sput(line, "memory  ");
    p = nput(p, (u32)(getfree() >> 20), 1);
    sput(p, "M free");
    api_draw_string(w, line, 24, 126, C_TEXT);
    api_draw_string(w, "(c) ilizavr & contributors", 24, 166, C_DIM);

    about_win = w;
}

static void reboot(void) { io_out(0x64, 0xFE); }

/* ---------- clock ---------- */

static u8 cmos(u8 r) { io_out(0x70, r); return io_in(0x71); }
static u8 bcd(u8 v) { return (v >> 4) * 10 + (v & 15); }

static const char *day_name(u32 d) {
    switch (d) {
        case 0: return "Sun"; case 1: return "Mon"; case 2: return "Tue"; case 3: return "Wed";
        case 4: return "Thu"; case 5: return "Fri"; default: return "Sat";
    }
}

static const char *mon_name(u32 m) {
    switch (m) {
        case 1: return "Jan"; case 2: return "Feb"; case 3: return "Mar"; case 4: return "Apr";
        case 5: return "May"; case 6: return "Jun"; case 7: return "Jul"; case 8: return "Aug";
        case 9: return "Sep"; case 10: return "Oct"; case 11: return "Nov"; case 12: return "Dec";
        default: return "???";
    }
}

static void read_clock(void) {
    while (cmos(0x0A) & 0x80) {}
    u8 s = cmos(0), mi = cmos(2), h = cmos(4), d = cmos(7), mo = cmos(8), y = cmos(9);
    if (!(cmos(0x0B) & 4)) {
        s = bcd(s); mi = bcd(mi); h = bcd(h & 0x7F); d = bcd(d); mo = bcd(mo); y = bcd(y);
    }

    u32 Y = 2000 + y, M = mo;
    if (M < 3) { M += 12; Y--; }
    u32 K = Y % 100, J = Y / 100;
    u32 z = (d + 13 * (M + 1) / 5 + K + K / 4 + J / 4 + 5 * J) % 7; // 0 = Saturday

    char *p = clock_str;
    p = sput(p, day_name((z + 6) % 7));
    p = sput(p, " ");
    p = nput(p, d, 1);
    p = sput(p, " ");
    p = sput(p, mon_name(mo));
    p = sput(p, "  ");
    p = nput(p, h, 2);
    p = sput(p, ":");
    p = nput(p, mi, 2);
    p = sput(p, ":");
    nput(p, s, 2);

    p = sput(mem_str, "RAM ");
    p = nput(p, (u32)(getfree() >> 20), 1);
    sput(p, "M free");
}

/* ---------- layout & hit tests ---------- */

static const char *app_name(void) { return active ? active->name : "Desktop"; }

static void layout_bar(void) {
    menu_x[0] = 12; menu_w[0] = 8;
    s32 x = 36 + (s32)strlen(app_name()) * 8 + 24;
    menu_x[1] = x; menu_w[1] = 32;
    x += 32 + 24;
    menu_x[2] = x; menu_w[2] = 48;
}

static void layout_dock(void) {
    s32 x = PAD;
    bool sep = 0;
    dk_n = 0;
    for (int a = 0; a < NAPPS; a++) {
        dk_x[dk_n] = x; dk_win[dk_n] = 0; dk_n++;
        x += TILE + GAP;
    }
    for (u32 i = 0; i < wn; i++) {
        if (!zo[i]->invisible) continue;
        if (!sep) { x += 8; sep = 1; }
        dk_x[dk_n] = x; dk_win[dk_n] = zo[i]; dk_n++;
        x += TILE + GAP;
    }
    dk_w = x - GAP + PAD;
    dk_x0 = (SW - dk_w) / 2;
    dk_y0 = SH - 4 - DOCK_H;
}

static int title_hit(void) {
    if (my >= BAR_H) return -1;
    for (int m = 0; m < 3; m++)
        if (mx >= menu_x[m] - 8 && mx < menu_x[m] + menu_w[m] + 8) return m;
        return -1;
}

static const char *menu_item(int m, int i, int *kind) {
    *kind = 0;
    if (m == 0) {
        switch (i) {
            case 0: return "About zhirOS";
            case 1: *kind = 1; return "";
            case 2: return "Terminal";
            case 3: return "Pong";
            case 4: return "Doom";
            case 5: *kind = 1; return "";
            case 6: return "Restart";
            default: return 0;
        }
    }
    if (m == 1) {
        if (i > 2) return 0;
        if (!active) *kind = 2;
        if (i == 0) return "Close Window";
        if (i == 1) return "Minimize";
        return active && active->full_screen ? "Unzoom" : "Zoom";
    }
    if (wn == 0) {
        if (i) return 0;
        *kind = 2;
        return "No Windows";
    }
    if (i >= (int)wn) return 0;
    return zo[wn - 1 - i]->name;
}

static s32 menu_height(int m) {
    s32 h = 8;
    int k;
    for (int i = 0; menu_item(m, i, &k); i++) h += k == 1 ? 9 : 20;
    return h;
}

// >=0 item, -2 inside menu but not clickable, -1 outside
static int menu_hit(void) {
    int m = menu_open;
    s32 x0 = menu_x[m] - 8, y = BAR_H - 1 + 4;
    if (mx < x0 || mx >= x0 + MENU_W || my < BAR_H - 1 || my >= BAR_H - 1 + menu_height(m)) return -1;
    int k;
    for (int i = 0; menu_item(m, i, &k); i++) {
        s32 h = k == 1 ? 9 : 20;
        if (my >= y && my < y + h) return k == 0 ? i : -2;
        y += h;
    }
    return -2;
}

static int dock_hit(void) {
    s32 ty = dk_y0 + 6;
    if (my < ty || my >= ty + TILE) return -1;
    for (int i = 0; i < dk_n; i++) {
        s32 tx = dk_x0 + dk_x[i];
        if (mx >= tx && mx < tx + TILE) return i;
    }
    return -1;
}

static Window *win_at(s32 x, s32 y) {
    for (int i = (int)wn - 1; i >= 0; i--) {
        Window *w = zo[i];
        if (w->invisible || !w->buffer) continue;
        s32 rows = win_rows(w);
        if (x >= w->x - 1 && x < w->x + w->width + 1 && y >= w->y - 1 && y < w->y + rows + 1) return w;
    }
    return 0;
}

/* ---------- composition ---------- */

static void blit(Window *w, s32 rows) {
    s32 sx = w->x, srcx = 0, cw = w->width;
    if (sx < 0) { srcx = -sx; cw += sx; sx = 0; }
    if (sx + cw > SW) cw = SW - sx;
    if (cw <= 0) return;
    for (s32 r = 0; r < rows; r++) {
        s32 sy = w->y + r;
        if (sy < 0 || sy >= SH) continue;
        cpy32(back + sy * PP + sx, w->buffer + r * w->width + srcx, cw);
    }
}

static void draw_window(Window *w) {
    s32 rows = win_rows(w);
    if (rows <= 0) return;
    draw_title(w);

    sdither(w->x + w->width + 1, w->y + 4, 4, rows + 1);
    sdither(w->x + 4, w->y + rows + 1, w->width - 3, 4);
    blit(w, rows);
    sframe(w->x - 1, w->y - 1, w->width + 2, rows + 2, w == active ? C_FRAME_ACT : C_FRAME);

    for (s32 d = 3; d <= 9; d += 3)
        for (s32 t = 0; t <= d; t++)
            sfill(w->x + w->width - 2 - t, w->y + rows - 2 - (d - t), 1, 1, 0xFF5A5A5A);
}

static void draw_bar(void) {
    sfill(0, 0, SW, BAR_H, C_BAR);
    sfill(0, BAR_H - 1, SW, 1, C_LINE);

    for (int m = 0; m < 3; m++) {
        if (menu_open == m) sfill(menu_x[m] - 8, 0, menu_w[m] + 16, BAR_H - 1, C_SEL);
        else if (menu_open < 0 && hover_id == 2000 + m) sfill(menu_x[m] - 8, 0, menu_w[m] + 16, BAR_H - 1, 0xFF222222);
    }

    sstrb(menu_x[0], 3, "\xFE", menu_open == 0 ? C_WHITE : C_ACC);
    sstrb(36, 3, app_name(), C_WHITE);
    sstr(menu_x[1], 3, "File", menu_open == 1 ? C_WHITE : C_TEXT);
    sstr(menu_x[2], 3, "Window", menu_open == 2 ? C_WHITE : C_TEXT);

    s32 cx = SW - 12 - (s32)strlen(clock_str) * 8;
    sstr(cx, 3, clock_str, C_TEXT);
    schar(cx - 16, 3, 0xB3, C_LINE);
    sstr(cx - 24 - (s32)strlen(mem_str) * 8, 3, mem_str, C_DIM);
}

static void draw_menu(void) {
    int m = menu_open;
    s32 x0 = menu_x[m] - 8, y0 = BAR_H - 1, h = menu_height(m);
    int hv = hover_id >= 3000 ? hover_id - 3000 : -1;

    sdither(x0 + MENU_W, y0 + 4, 4, h);
    sdither(x0 + 4, y0 + h, MENU_W - 4, 4);
    sfill(x0, y0, MENU_W, h, C_PANEL);
    sframe(x0, y0, MENU_W, h, 0xFF4A4A4A);

    s32 y = y0 + 4;
    int k;
    const char *s;
    for (int i = 0; (s = menu_item(m, i, &k)); i++) {
        if (k == 1) {
            sfill(x0 + 8, y + 4, MENU_W - 16, 1, C_LINE);
            y += 9;
            continue;
        }
        u32 col = k == 2 ? 0xFF555555 : C_TEXT;
        if (i == hv && k == 0) {
            sfill(x0 + 2, y, MENU_W - 4, 20, C_SEL);
            col = C_WHITE;
        }
        if (m == 2 && wn) {
            Window *w = zo[wn - 1 - i];
            if (w == active) schar(x0 + 10, y + 2, 0xFE, C_ACC);
            if (w->invisible) sstr(x0 + MENU_W - 48, y + 2, "min", C_DIM);
        }
        sstr(x0 + 26, y + 2, s, col);
        y += 20;
    }
}

static void draw_tile(int i, s32 x, s32 y, bool hov) {
    Window *w = dk_win[i];
    if (!w) {
        if (i == 0) {
            sfill(x, y, TILE, TILE, 0xFF000000);
            sstr(x + 10, y + 10, ">_", C_ACC);
        } else if (i == 1) {
            sfill(x, y, TILE, TILE, 0xFF1C1C1C);
            sfill(x + 11, y + 7, 14, 2, C_WHITE);
            sfill(x + 11, y + 27, 14, 2, C_WHITE);
            sfill(x + 20, y + 16, 4, 4, C_ACC);
        } else {
            sfill(x, y, TILE, TILE, 0xFF3B82F6);
            sfill(x, y + 24, TILE, 12, 0xFF22A05A);
            sfill(x + 6, y + 18, 10, 6, 0xFF1E7A45);
            sfill(x + 23, y + 6, 6, 6, 0xFFFDE047);
        }
        if (find_win(app_wname(i))) sfill(x + 16, y + TILE + 3, 4, 2, C_ACC);
    } else {
        char t[3];
        t[0] = w->name[0];
        t[1] = w->name[0] ? w->name[1] : 0;
        t[2] = 0;
        sfill(x, y, TILE, TILE, 0xFF202020);
        sfill(x, y, TILE, 7, 0xFF333333);
        sfill(x + 3, y + 2, 3, 3, 0xFFFF5F57);
        sfill(x + 8, y + 2, 3, 3, 0xFFFEBC2E);
        sfill(x + 13, y + 2, 3, 3, 0xFF28C840);
        sstr(x + 10, y + 13, t, C_TEXT);
    }
    sframe(x, y, TILE, TILE, hov ? C_ACC : 0xFF444444);
}

static void draw_dock(void) {
    sdither(dk_x0 + dk_w, dk_y0 + 4, 4, DOCK_H);
    sfill(dk_x0, dk_y0, dk_w, DOCK_H, C_PANEL);
    sframe(dk_x0, dk_y0, dk_w, DOCK_H, C_FRAME);

    if (dk_n > NAPPS) sfill(dk_x0 + dk_x[NAPPS] - 8, dk_y0 + 8, 1, DOCK_H - 16, C_FRAME);

    for (int i = 0; i < dk_n; i++)
        draw_tile(i, dk_x0 + dk_x[i], dk_y0 + 6, hover_id == 1000 + i);
}

static void draw_tooltip(int i) {
    const char *s = dk_win[i] ? dk_win[i]->name : app_label(i);
    s32 w = (s32)strlen(s) * 8 + 16;
    s32 x = dk_x0 + dk_x[i] + TILE / 2 - w / 2;
    s32 y = dk_y0 - 28;
    sdither(x + 3, y + 3, w, 22);
    sfill(x, y, w, 22, C_BAR);
    sframe(x, y, w, 22, C_FRAME_ACT);
    sstr(x + 8, y + 3, s, C_TEXT);
}

static void draw_cursor(void) {
    for (s32 r = 0; r < 17; r++)
        for (s32 c = 0; c < 12; c++) {
            char p = cursor_shape[r][c];
            if (p == '#') sfill(mx + c, my + r, 1, 1, 0xFF000000);
            else if (p == 'o') sfill(mx + c, my + r, 1, 1, C_WHITE);
        }
}

static void compose(void) {
    tgt = back;
    cpy32(back, bg, PP * SH);
    for (u32 i = 0; i < wn; i++) {
        Window *w = zo[i];
        if (!w->invisible && w->buffer) draw_window(w);
    }
    draw_bar();
    draw_dock();
    if (menu_open >= 0) draw_menu();
    if (hover_id >= 1000 && hover_id < 2000) draw_tooltip(hover_id - 1000);
    draw_cursor();
}

static void dirty(s32 x, s32 y, s32 w, s32 h) {
    if (w <= 0 || h <= 0) return;
    if (!dv) { dx0 = x; dy0 = y; dx1 = x + w; dy1 = y + h; dv = 1; return; }
    if (x < dx0) dx0 = x;
    if (y < dy0) dy0 = y;
    if (x + w > dx1) dx1 = x + w;
    if (y + h > dy1) dy1 = y + h;
}

static void flush(void) {
    if (!dv) return;
    dv = 0;
    s32 x = dx0, y = dy0, w = dx1 - dx0, h = dy1 - dy0;
    if (!clip(&x, &y, &w, &h)) return;
    for (s32 r = 0; r < h; r++) cpy32(vram + (y + r) * PP + x, back + (y + r) * PP + x, w);
}

/* ---------- input ---------- */

static void do_item(int m, int i) {
    if (m == 0) {
        if (i == 0) about_open();
        else if (i >= 2 && i <= 4) launch(i - 2);
        else if (i == 6) reboot();
    } else if (m == 1) {
        if (!active) return;
        if (i == 0) win_close(active);
        else if (i == 1) win_minimize(active);
        else win_zoom(active);
    } else if (i < (int)wn) {
        focus(zo[wn - 1 - i]);
    }
}

static void on_press(void) {
    if (menu_open >= 0) {
        int it = menu_hit();
        if (it != -1) {
            int m = menu_open;
            menu_open = -1;
            full = 1;
            if (it >= 0) do_item(m, it);
            return;
        }
        int t = title_hit();
        menu_open = (t >= 0 && t != menu_open) ? t : -1;
        full = 1;
        return;
    }

    int t = title_hit();
    if (t >= 0) { menu_open = t; full = 1; return; }
    if (my < BAR_H) return;

    int d = dock_hit();
    if (d >= 0) {
        if (dk_win[d]) focus(dk_win[d]);
        else launch(d);
        return;
    }

    Window *w = win_at(mx, my);
    if (!w) return;
    focus(w);

    s32 lx = mx - w->x, ly = my - w->y;
    if (ly < TITLE_H) {
        if (ly >= 3 && ly < 17) {
            for (int b = 0; b < 3; b++) {
                if (lx >= 6 + b * 16 && lx < 20 + b * 16) {
                    if (b == 0) win_close(w);
                    else if (b == 1) win_minimize(w);
                    else win_zoom(w);
                    return;
                }
            }
        }
        drag_w = w;
        drag_dx = lx;
        drag_dy = ly;
        return;
    }

    s32 mode = 0;
    if (lx >= w->width - 10 && ly >= w->height - 10) mode = 3;
    else if (lx >= w->width - 3) mode = 1;
    else if (ly >= w->height - 3) mode = 2;
    if (mode) {
        rs_w = w; rs_mode = mode;
        rs_mx = mx; rs_my = my;
        rs_w0 = w->width; rs_h0 = w->height;
    }
}

static void on_hold(void) {
    if (drag_w) {
        s32 nx = mx - drag_dx, ny = my - drag_dy;
        if (ny < BAR_H + 1) ny = BAR_H + 1;
        if (ny > SH - 24) ny = SH - 24;
        if (nx < -(drag_w->width - 48)) nx = -(drag_w->width - 48);
        if (nx > SW - 48) nx = SW - 48;
        if (nx != drag_w->x || ny != drag_w->y) {
            drag_w->x = nx;
            drag_w->y = ny;
            drag_w->full_screen = 0;
            full = 1;
        }
    }
    if (rs_w) {
        s32 nw = rs_w0, nh = rs_h0;
        if (rs_mode & 1) nw += mx - rs_mx;
        if (rs_mode & 2) nh += my - rs_my;
        win_resize(rs_w, nw, nh);
    }
}

static void update_hover(void) {
    int h = 0;
    Window *bw = 0;

    if (menu_open >= 0) {
        int it = menu_hit();
        if (it >= 0) h = 3000 + it;
        int t = title_hit();
        if (t >= 0 && t != menu_open) { menu_open = t; full = 1; }
    }
    if (!h) {
        int t = title_hit();
        if (t >= 0) h = 2000 + t;
        else {
            int d = dock_hit();
            if (d >= 0) h = 1000 + d;
            else if (my >= BAR_H) {
                Window *w = win_at(mx, my);
                if (w) {
                    s32 lx = mx - w->x, ly = my - w->y;
                    if (lx >= 4 && lx < 54 && ly >= 1 && ly < 19) bw = w;
                }
            }
        }
    }

    if (h != hover_id || bw != hover_btn_win) {
        hover_id = h;
        hover_btn_win = bw;
        full = 1;
    }
}

static void wm_main(void) {
    s32 omx = -1, omy = -1;
    bool ol = 0;
    u32 last_frame = 0, last_sec = 0xFFFFFFFF;
    full = 1;

    while (1) {
        if (mouse_xf) {
            mx = mouse_xf();
            my = mouse_yf();
        }
        if (mx >= SW) mx = SW - 1;
        if (my >= SH) my = SH - 1;
        bool l = mouse_lf ? mouse_lf() : 0;

        layout_bar();
        layout_dock();

        if (l && !ol) on_press();
        if (l) on_hold();
        if (!l && ol) { drag_w = 0; rs_w = 0; }
        ol = l;

        layout_bar();
        layout_dock();
        update_hover();

        u32 now = getticks();
        if (now / 1000 != last_sec) {
            last_sec = now / 1000;
            read_clock();
            bar_dirty = 1;
        }

        bool moved = mx != omx || my != omy;
        if ((full || app_dirty || bar_dirty || moved) && now != last_frame) {
            compose();
            if (full) {
                dirty(0, 0, SW, SH);
            } else {
                if (app_dirty)
                    for (u32 i = 0; i < wn; i++) {
                        Window *w = zo[i];
                        if (!w->invisible) dirty(w->x - 1, w->y - 1, w->width + 7, win_rows(w) + 7);
                    }
                    if (bar_dirty) dirty(0, 0, SW, BAR_H);
                    if (moved) {
                        dirty(omx, omy, 12, 17);
                        dirty(mx, my, 12, 17);
                    }
            }
            flush();
            full = app_dirty = bar_dirty = 0;
            omx = mx;
            omy = my;
            last_frame = now;
        }
        yield();
    }
}

INIT void init(void *_resolve_function(char *name))
{
    api_init(_resolve_function);
    mouse_xf = _resolve_function("_get_mouse_x");
    mouse_yf = _resolve_function("_get_mouse_y");
    mouse_lf = _resolve_function("_get_mouse_left");

    register_function("_window_create", api_window_create, "Create window");
    register_function("_window_render", api_window_render, "Render window");
    register_function("_clearframe_win", api_clear, "Clearframe windows");
    register_function("_window_fill_rect", api_fill, "Draw window fill rect");
    register_function("_draw_string", api_draw_string, "Draw string");
    register_function("_draw_char", api_draw_char, "Draw char");
    register_function("_draw_char_bg", api_draw_char_bg, "Draw char with background");
    register_function("_draw_border_window", api_border, "Draw border for window");
    register_function("_compositor_present", api_compositor_present, "Request redraw");
    register_function("_window_is_active", api_window_is_active, "Check active window");
    register_function("_desktop_fill_rect", api_desk_fill, "Draw rect on desktop");
    register_function("_desktop_draw_string", api_desk_str, "Draw string on desktop");
    register_function("_window_resize", win_resize, "Resize window");

    fb = fbcon_stop();
    SW = fb->screen_width;
    SH = fb->screen_height;
    PP = fb->screen_pitch / 4;
    vram = (u32 *)fb->fb_addr;

    back = alloc(PP * SH * 4);
    bg = alloc(PP * SH * 4);
    if (!back || !bg) return;

    tgt = bg;
    build_wallpaper();
    tgt = back;

    create_task("zhirmac", wm_main);
}
