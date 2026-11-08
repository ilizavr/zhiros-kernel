/*
 * Copyright (c) 2026 ilizavr & TINERKOTL & yellowhat
 * SPDX-License-Identifier: MIT
 */

#include "WM_types.h"
#include "../lib/zhirapi.h"

Window* (*window_create)(char *name);
void (*window_render)(Window *window);
void (*window_fill_rect)(Window *window, u32 x, u32 y, u32 width, u32 height, u32 color);
void (*draw_string)(Window *window, const char *str, u32 x, u32 heght, u32 color);
void (*draw_border_window)(Window *window, u32 color);
void (*compositor_present)();
bool (*window_is_active)(Window *window);
void (*draw_char)(Window *window, u8 code, u32 x, u32 y, u32 color);
char (*find_letter)(void);

struct task *TE_task;

struct Window *TextEditor;

#define FONT_W 8
#define FONT_H 16
#define FRAME_DIV 10000
#define BORDER 1 // толщина рамки
#define NUMBER_LINE 31
#define TITLE_H 20
#define FG_COLOR 0xBBBBBBBB

u32 color = 0;
u32 curx = 0, cury = 36;
u32 cur_row = 0, cur_col = 0;
u32 scroll_top = 0;
u32 frame = 0;
volatile u32 fb_addr,fb_size,fb_size_withoutlastline,fb_lastline_addr,fb_onelinesize,fb_secondline_addr, screen_height;
bool dirty = true;
bool last_phase;
bool phase = false;
bool changed = false;
bool prev_keys[128];

struct file *text_file;

typedef struct Line {
    char *data;
    size_t len;
    size_t capacity;
} Line;

typedef struct TextBuffer {
    struct Line *lines;
    size_t count;
    size_t capacity;
} TextBuffer;

static TextBuffer tb;

static bool reserve_line(size_t need)
{
    size_t new_capacity = tb.capacity ? tb.capacity * 2 : 16;
    Line *new_lines;
    size_t i;

    while (new_capacity < need)
        new_capacity *= 2;

    if (tb.capacity >= need)
        return true;

    new_lines = alloc((u32)(new_capacity * sizeof(Line)));
    if (!new_lines)
        return false;

    for (i = 0; i < tb.count; i++)
        new_lines[i] = tb.lines[i];

    if (tb.lines)
        free(tb.lines);

    tb.lines = new_lines;
    tb.capacity = new_capacity;
    return true;
}

static bool reserve_line_data(Line *line, size_t need)
{
    size_t new_capacity = line->capacity ? line->capacity * 2 : 16;
    char *new_data;
    size_t i;

    while (new_capacity < need)
        new_capacity *= 2;

    if (line->capacity >= need)
        return true;

    new_data = alloc((u32)new_capacity);
    if (!new_data)
        return false;

    for (i = 0; i < line->len; i++)
        new_data[i] = line->data[i];

    if (line->data)
        free(line->data);

    line->data = new_data;
    line->capacity = new_capacity;
    return true;
}

static u32 win_cols(void) {
    if (!TextEditor || TextEditor->width < 2 * BORDER + FONT_W)
        return 0;
    return (TextEditor->width - 2 * BORDER) / (FONT_W + 1);
}

static u32 win_rows(void) {
    if (!TextEditor || TextEditor->height < TITLE_H + BORDER + FONT_H)
        return 0;
    return (TextEditor->height - TITLE_H - BORDER) / FONT_H;
}

static void follow_cursor(void)
{
    u32 rows = win_rows();

    if (rows == 0)
        return;
    if (cur_row <= scroll_top && cur_row >= 0)
        scroll_top = cur_row;
    if (cur_row >= scroll_top + rows)
        scroll_top = cur_row + 1 - rows;
}

static void clearframe_win(Window *window, u32 color) {
    if (!window || !window->buffer || window->width < 3 || window->height <= 20 + 1)
        return;

    for (u32 y = 20; y < (u32)window->height - 1; y++) {
        for (u32 x = 1; x < (u32)window->width - 1; x++) {
            window->buffer[y * window->width + x] = color;
        }
    }
}

static bool new_line(){
    if (!reserve_line(tb.count + 1))
        return 0;

    cur_col = 0;
    cur_row = tb.count;

    tb.lines[cur_row].data = 0;
    tb.lines[cur_row].len = 0;
    tb.lines[cur_row].capacity = 0;
    tb.count++;

    follow_cursor();
    return true;
}

static void backspace(){
    Line *line = &tb.lines[cur_row];
    if (cur_col > 0){
        line->data[cur_col] = 0;
        cur_col--;
        line->len--;
    }
    if (cur_col == 0 && cur_row > 0){
        cur_row--;
        cur_col = tb.lines[cur_row].len;
        tb.count--;
    }
    follow_cursor();
    dirty = true;
}

static void put_key(char code)
{
    u32 cols = win_cols();
    Line *line;

    if (cols == 0)
        return;

    if (cur_col >= cols && !new_line())
        return;

    line = &tb.lines[cur_row];
    if (cur_col > line->len)
        cur_col = line->len;

    if (!reserve_line_data(line, line->len + 1))
        return;

    for (size_t i = line->len; i > cur_col; i--)
        line->data[i] = line->data[i - 1];

    line->data[cur_col] = code;
    line->len++;
    cur_col++;

    follow_cursor();
    dirty = true;
}

static bool save_text(void) {
    u32 total = 0;
    u32 offset = 0;
    char *data;

    if (!text_file || !text_file->write)
        return false;

    for (u32 row = 0; row < tb.count; row++){
        total += tb.lines[row].len;
        if (row + 1 < tb.count)
            total++;
    }

    data = alloc(total ? total : 1);
    if (!data)
        return false;

    for (u32 row = 0; row < tb.count; row++) {
        Line *line = &tb.lines[row];

        if (line->len){
            memcpy(data + offset, line->data, line->len);
            offset += line->len;
        }

        if (row + 1 < tb.count)
            data[offset++] = '\n';
    }

    if (total && text_file->write(text_file, data, total, 0) != total) {
        free(data);
        return false;
    }

    free(data);
    return true;
}

static void move_up_down(bool up) {
    if (up && cur_row >= scroll_top && cur_row > 0)
        cur_row--;

    if (!up && cur_row + 1 < tb.count)
        cur_row++;

    if (cur_col > tb.lines[cur_row].len)
        cur_col = tb.lines[cur_row].len;

    follow_cursor();
    dirty = true;
}

static void move_left_right(bool left) {
    if (left && cur_col > 0)
        cur_col--;

    if (!left && cur_col <= tb.lines[cur_row].len)
        cur_col++;

    follow_cursor();
    dirty = true;
}

static void poll_keyboard(void) {
    bool *key_state_matrix = get_key_state_matrix();
    bool shift = key_state_matrix[0x2A] || key_state_matrix[0x36];
    bool ctrl = key_state_matrix[0x1D];
    u32 code;

    for (code = 0; code < 128; code++) {
        bool pressed = key_state_matrix[code];
        bool active = prev_keys[code];

        prev_keys[code] = pressed;

        if (pressed && !active) {
            char ch = shift ? keyboard_map_shift[code] : keyboard_map[code];
            if (code == 0x0F) {
                u32 space = 4 - (cur_col % 4);
                while (space--)
                    put_key(' ');
                continue;
            }
            if (code == 0x1C) {
                new_line();
                continue;
            }
            if (code == 0x0E) {
                backspace();
                continue;
            }
            if (code == 0x3A) {
                shift = !shift;
                continue;
            }
            if (ctrl && code == 0x1F) {
                save_text();
                continue;
            }
            if (code == 0x48) {
                move_up_down(true);
                continue;
            }
            if (code == 0x50) {
                move_up_down(false);
                continue;
            }
            if (code == 0x4B) {
                move_left_right(true);
                continue;
            }
            if (code == 0x4D) {
                move_left_right(false);
                continue;
            }
            if (ch >= 32 && ch <= 126) {
                put_key(ch);
                continue;
            }
        }
    }
}


static void te_reset_state(void) {
    u32 i;

    tb.lines = 0;
    tb.count = 0;
    tb.capacity = 0;

    if(!reserve_line(16))
        return;

    for (i = 0; i < tb.capacity; i++){
        tb.lines[i].data = 0;
        tb.lines[i].len = 0;
        tb.lines[i].capacity = 0;
    }
    tb.count = 1;
    cur_row = 0;
    cur_col = 0;
    scroll_top = 0;
    frame = 0;
    bool dirty = true;
    for (i = 0; i < 128; i++)
        prev_keys[i] = false;
}

static void redraw(void) {
    u32 px = NUMBER_LINE, py = TITLE_H;
    u32 px_number = BORDER;
    u32 rows = win_rows();
    u32 cols = win_cols();

    window_fill_rect(TextEditor, BORDER, TITLE_H, TextEditor->width - 2 * BORDER, TextEditor->height - TITLE_H - BORDER, 0xFF333333);

    for (u32 r = 0; r < rows; r++) {
        u32 source_row = scroll_top + r;
        if (source_row >= tb.count)
            break;

        u32 number = source_row + 1; // Нумерация начинается с 1
        char digits[10];
        u32 digit_count = 0;

        do {
            digits[digit_count++] = (char)('0' + number % 10);
            number /= 10;
        } while (number > 0);

        for (u32 d = 0; d < digit_count; d++) {
            draw_char(TextEditor, (u8)digits[digit_count - 1 - d], px_number + d * (FONT_W + 1), py + r * FONT_H, 0xBBAAAAAA);
        }

        Line *line = &tb.lines[source_row];
        for (u32 c = 0; c < line->len && c < cols; c++) {
            draw_char(TextEditor, (u8)line->data[c], px + c * (FONT_W + 1), py + r * FONT_H, 0xFFFFFFFF);
        }
    }

    if (cur_row >= scroll_top && cur_row < scroll_top + rows && cur_row < tb.count && cur_col < cols){
        u32 cx = px + cur_col * (FONT_W + 1);
        u32 cy = py + (cur_row - scroll_top) * FONT_H;

        if (phase) {
            if (cur_col < (u32)tb.lines[cur_row].len)
                window_fill_rect(TextEditor, cx, cy + FONT_H - 2, FONT_W, 2, FG_COLOR);
            else
                window_fill_rect(TextEditor, cx, cy, FONT_W, FONT_H, FG_COLOR);
        }
    }

    dirty = false;
}

static void resize_text_editor(Window *window, s32 old_width, s32 old_height) {
    if (!window || !window->buffer || window->width < 16 || window->height < 36)
        return;

    dirty = false;
}

static bool load_text(struct file *file) {
    if (!file || !file->read || !file->getsize)
        return false;

    u32 size = file->getsize(file);
    if (size == 0)
        return true;

    char *data = alloc(size);
    if (!data)
        return false;

    u32 bytes = file->read(file, data, size, 0);
    for (u32 i = 0; i < bytes; i++) {
        if (data[i] == '\n'){
            if (!new_line()) {
                free(data);
                return false;
            }
        } else if (data[i] != '\r') {
            Line *line = &tb.lines[cur_row];
            if (!reserve_line_data(line, line->len + 1)) {
                free(data);
                return false;
            }
            line->data[line->len++] = data[i];
            cur_col = line->len;
        }
    }

    free(data);
    follow_cursor();
    dirty = true;
    return true;
}

static void TE() {
    TextEditor = window_create("ZhirTE");
    if (!TextEditor)
        return;
    TextEditor->window_on_changed = 1;
    TextEditor->task = TE_task;
    TextEditor->on_resize = resize_text_editor;

    window_render(TextEditor);
    clearframe_win(TextEditor, 0xFF333333);

    te_reset_state();

    // char letter = find_letter();
    char letter = 'B';
    if (!letter)
        return;

    text_file = open(letter, "/python.py");
    if (!text_file)
        text_file = create_file(letter, "/python.py");

    if (!text_file)
        return;


    load_text(text_file);

    while(1) {

        screen_height = TextEditor->height;

        fb_addr = (i_ptr)(TextEditor->buffer+TextEditor->width*20);
        fb_size = (i_ptr)(TextEditor->buffer_size*4-TextEditor->width*80);
        fb_size_withoutlastline = (i_ptr)(TextEditor->width*(screen_height-16)*4);
        fb_lastline_addr = fb_addr+fb_size_withoutlastline;
        fb_onelinesize = TextEditor->width*16*4;
        fb_secondline_addr = fb_addr+fb_onelinesize;

        if (window_is_active(TextEditor))
            poll_keyboard();

        frame = getticks();
        phase = ((frame / 400) & 1) != 0;
        if (dirty || phase != last_phase){
            last_phase = phase;
            redraw();
            compositor_present();
        }
        yield();

    }
}

INIT void init(void* _resolve_function(char* name))
{
    api_init(_resolve_function);
    window_create = _resolve_function("_window_create");
    window_render = _resolve_function("_window_render");
    window_fill_rect = _resolve_function("_window_fill_rect");
    draw_string = _resolve_function("_draw_string");
    compositor_present = _resolve_function("_compositor_present");
    window_is_active = _resolve_function("_window_is_active");
    draw_char = _resolve_function("_draw_char");
    find_letter = _resolve_function("_find_letter");

    TE_task = create_task("ZhirTE", TE);
}
