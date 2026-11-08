/*
 * Copyright (c) 2026 ilizavr & TINERKOTL & yellowhat
 * SPDX-License-Identifier: MIT
 *
 * ZhirTE — полноценный текстовый редактор для ZhirOS.
 * Написан с нуля на базе примера TE и API ядра (zhirapi.h). Один файл, C99.
 *
 * Возможности:
 *   - корректный буфер строк: вставка/удаление, разрыв и слияние строк;
 *   - Undo/Redo (Ctrl+Z / Ctrl+Y) с группировкой набора текста;
 *   - выделение (Shift+стрелки/Home/End/PgUp/PgDn, Ctrl+A), буфер обмена
 *     Ctrl+C / Ctrl+X / Ctrl+V (внутренний, без выделения копируется строка);
 *   - поиск Ctrl+F (без учёта регистра), F3 — следующее вхождение;
 *   - переход на строку Ctrl+G; Ctrl+Backspace — удалить слово слева;
 *   - номера строк, строка состояния (файл, изменённость, Ln/Col, режим INS/OVR);
 *   - подсветка синтаксиса ZhirScript и Python (автоопределение языка);
 *   - вертикальная и горизонтальная прокрутка, автоповтор удерживаемых клавиш;
 *   - файлы: старт с /first.py (как в примере), Ctrl+O — открыть,
 *     Ctrl+S — сохранить, Ctrl+Shift+S / сохранение без пути — «сохранить как»,
 *     Ctrl+Q — выход с запросом сохранения. Диалоги рисуются прямо в окне.
 *   - F1 — справка по клавишам.
 *
 * Интеграция: положить рядом с примером TE (каталог app/), сборка без изменений.
 * Все изменения буфера двухфазные: сначала выделение памяти, потом перестройка,
 * поэтому при нехватке памяти состояние не портится (сообщение "Out of memory").
 *
 * Допущения о ядре (как в исходном примере):
 *   - getticks() возвращает тики ~миллисекунды (BLINK/REPEAT от них);
 *   - шрифт 8x16, содержимое только ASCII 32..126, поэтому UI на английском;
 *   - struct file->write(file, data, size, 0) переписывает файл с нуля;
 *     усечения до длины записи нет (если файл был длиннее, хвост остаётся).
 */

#include "WM_types.h"
#include "../lib/zhirapi.h"

/* ===================== конфигурация ===================== */

#define FONT_W        8
#define FONT_H        16
#define CELL_W        (FONT_W + 1)   /* шаг символа по X, как в примере */
#define TITLE_H       20             /* высота заголовка окна */
#define BORDER        1              /* толщина рамки */
#define TAB_WIDTH     4
#define MAX_PATH      60
#define MAX_QUERY     48
#define UNDO_LIMIT    256
#define REPEAT_DELAY  350            /* тиков до автоповтора */
#define REPEAT_RATE   40             /* тиков между повторами */
#define GROUP_TIMEOUT 1200           /* пауза, разбивающая группу undo */
#define BLINK_HALF    400            /* полупериод мигания курсора */
#define MSG_TIME      2000           /* время показа сообщения в статусе */
#define SYN_MAX_COLS  320            /* предел колонок с подсветкой (кэш) */
#define SYN_SCAN_ROWS 300            /* строк буфера для определения языка */

/* палитра (тёмная тема, в духе VS Code Dark) */
#define C_BG        0xFF1E1E1E
#define C_CURLINE   0xFF2A2A2B
#define C_FG        0xFFD4D4D4
#define C_SEL       0xFF264F78
#define C_SEL_FG    0xFFFFFFFF
#define C_CUR       0xFFAEAFAD
#define C_GUT       0xFF858585
#define C_GUT_CUR   0xFFC8C8C8
#define C_STATUS_BG 0xFF007ACC
#define C_STATUS_FG 0xFFFFFFFF
#define C_PANEL     0xFF2D2D30
#define C_PANEL_BR  0xFF007ACC
#define C_SHADOW    0xFF141414

/* палитра подсветки синтаксиса (из _pfs, cli_NS.c) */
#define SY_KW      0xFFFFAA00u  /* ключевые слова — оранжевый          */
#define SY_TYPE    0xFF56C9E8u  /* типы u32/fp/str — голубой           */
#define SY_BUILT   0xFFC792EAu  /* встроенные функции — сиреневый      */
#define SY_FUNC    0xFF82AAFFu  /* имена функций — синий               */
#define SY_STR     0xFF8CE99Au  /* строки — зелёный                    */
#define SY_NUM     0xFFFFD866u  /* числа — жёлтый                      */
#define SY_COM     0xFF707070u  /* комментарии — серый                 */
#define SY_OP      0xFFFF7EB6u  /* операторы == && + — розовый         */
#define SY_PUNCT   0xFF8C8C8Cu  /* скобки/запятые/двоеточия — тусклый  */

/* скан-коды (set 1) */
#define SC_ESC    0x01
#define SC_BSP    0x0E
#define SC_TAB    0x0F
#define SC_ENTER  0x1C
#define SC_LCTRL  0x1D
#define SC_LSHIFT 0x2A
#define SC_ALT    0x38
#define SC_CAPS   0x3A
#define SC_F1     0x3B
#define SC_F3     0x3D
#define SC_HOME   0x47
#define SC_UP     0x48
#define SC_PGUP   0x49
#define SC_LEFT   0x4B
#define SC_RIGHT  0x4D
#define SC_END    0x4F
#define SC_DOWN   0x50
#define SC_PGDN   0x51
#define SC_INS    0x52
#define SC_DEL    0x53
#define SC_RSHIFT 0x36

#define MSG_NOMEM "Out of memory"

/* ===================== типы ===================== */

typedef struct Line {
    char  *data;
    size_t len;
    size_t capacity;
} Line;

typedef struct TextBuffer {
    Line  *lines;
    size_t count;
    size_t capacity;
} TextBuffer;

/* шаг undo: снимок строк [first_row, first_row+snap_count) ДО правки,
 * after_count — сколько строк занимает этот участок ПОСЛЕ правки. */
typedef struct UndoStep {
    size_t first_row;
    size_t snap_count;
    Line  *snap;
    size_t after_count;
    size_t cur_row;   /* курсор до правки (для undo) */
    size_t cur_col;
} UndoStep;

enum {
    GK_NONE = 0,   /* группа неактивна */
    GK_INS,        /* серия вставленных символов */
    GK_BS          /* серия backspace в строке */
};

enum {
    PM_NONE = 0,
    PM_OPEN,
    PM_SAVEAS,
    PM_CONFIRM,
    PM_GOTO
};

enum {
    CA_OPEN = 0,   /* подтверждение перед открытием другого файла */
    CA_QUIT        /* подтверждение перед выходом */
};

enum {
    SYN_PY = 0,    /* python */
    SYN_ZHIR       /* zhirscript (язык ОС, по умолчанию) */
};

/* ===================== API (как в примере) ===================== */

Window* (*window_create)(char *name);
void (*window_render)(Window *window);
void (*window_fill_rect)(Window *window, u32 x, u32 y, u32 width, u32 height, u32 color);
void (*draw_string)(Window *window, const char *str, u32 x, u32 y, u32 color);
void (*compositor_present)();
bool (*window_is_active)(Window *window);
void (*draw_char)(Window *window, u8 code, u32 x, u32 y, u32 color);
char (*find_letter)(void);

struct task *TE_task;
struct Window *TextEditor;

/* ===================== состояние ===================== */

static TextBuffer tb;
static size_t cur_row = 0, cur_col = 0;
static size_t scroll_top = 0, scroll_left = 0;
static bool   dirty = true;
static bool   last_phase = false;
static bool   phase = false;
static bool   ovr = false;          /* режим вставки/замены (Ins) */
static bool   caps = false;
static bool   closed = false;
static bool   modified = false;
static bool   prev_keys[128];

static size_t held_code = 0;        /* удерживаемая клавиша для автоповтора */
static u32    held_next = 0;
static u32    ticks_now = 0;

/* выделение */
static bool   sel_active = false;
static size_t sel_row = 0, sel_col = 0;   /* якорь */

/* буфер обмена */
static char  *clip = 0;
static size_t clip_len = 0;

/* undo / redo */
static UndoStep *ustack = 0; static size_t ucount = 0, ucap = 0;
static UndoStep *rstack = 0; static size_t rcount = 0, rcap = 0;
static int   group_kind = GK_NONE;
static size_t g_row = 0, g_col = 0;
static u32   g_ticks = 0;

/* поиск */
static char   query[MAX_QUERY + 1];
static size_t query_len = 0;
static bool   search_mode = false;
static bool   have_query = false;

/* диалоги */
static int    prompt_mode = PM_NONE;
static char   pbuf[MAX_PATH + 1];
static size_t plen = 0;
static int    confirm_action = CA_OPEN;
static bool   help_open = false;

/* статус */
static char status_msg[80];
static u32  msg_until = 0;

/* файл */
static struct file *g_file = 0;
static char g_disk = 0;
static char file_path[MAX_PATH + 1];
static bool has_path = false;

/* подсветка синтаксиса */
static int syn_lang = SYN_ZHIR;              /* язык текущего файла */
static u32 syn_colors[SYN_MAX_COLS];         /* кэш цветов видимой строки */

/* ===================== прототипы ===================== */

static void  *te_memcpy(void *dst, const void *src, size_t n);
static void  *te_memmove(void *dst, const void *src, size_t n);
static size_t te_strlen(const char *s);
static void   str_copy(char *dst, const char *src);
static bool   str_append(char *dst, size_t cap, const char *src);
static void   u32_to_str(u32 v, char *buf);
static void   u32_append(char *dst, size_t cap, u32 v);
static bool   nc_eq(char a, char b);
static bool   is_word_ch(char c);
static bool   te_memeq(const char *a, const char *b, size_t n);

static bool line_reserve(Line *ln, size_t need);
static bool line_build(Line *out, const char *p1, size_t n1,
                       const char *p2, size_t n2, const char *p3, size_t n3);
static bool line_copy_to(Line *dst, const Line *src);
static bool buf_ensure(size_t need);
static void buf_clear(void);
static bool buf_insert_empty_line(size_t row);
static bool buf_insert_char(size_t row, size_t col, char ch);
static void buf_delete_char(size_t row, size_t col);
static bool buf_split_line(size_t row, size_t col);
static bool buf_merge_with_next(size_t row);
static bool buf_insert_text(size_t row, size_t col, const char *s, size_t n, size_t *added);
static bool buf_replace(size_t row, size_t kill, const Line *src, size_t n);

static bool edit_begin(size_t first_row, size_t n, UndoStep *st);
static void edit_commit(UndoStep *st, size_t after_count);
static void edit_abort(UndoStep *st);
static void step_free(UndoStep *st);
static void stack_push(UndoStep **arr, size_t *cnt, size_t *cap, UndoStep *st);
static void free_undo_all(void);
static void free_redo_all(void);
static bool capture_region(size_t row, size_t n, Line **out);
static void do_undo(void);
static void do_redo(void);
static void close_group(void);

static void sel_norm(size_t *r1, size_t *c1, size_t *r2, size_t *c2);
static bool in_sel(size_t row, size_t col);
static bool delete_selection(void);
static bool edit_delete_range(size_t r1, size_t c1, size_t r2, size_t c2);
static void do_copy(bool cut);
static void do_paste(void);

static size_t win_cols(void);
static size_t win_rows(void);
static size_t text_rows(void);
static size_t gutter_width(void);
static size_t text_cols(void);
static void   follow_cursor(void);
static void   cursor_to(size_t row, size_t col, bool select);
static void   move_left(bool select);
static void   move_right(bool select);
static void   move_up(bool select);
static void   move_down(bool select);
static void   move_word_left(bool select);
static void   move_word_right(bool select);
static void   move_page(bool up, bool select);

static void do_type_char(char ch);
static void do_insert_char(char ch);
static void do_backspace(void);
static void do_delete(void);
static void do_enter(void);
static void do_tab(void);

static void set_msg(const char *s);
static bool save_current(void);
static void load_from(struct file *f);
static void do_open_path(const char *path);
static void do_saveas_path(const char *path);
static void start_open(void);
static void start_saveas(void);
static void start_quit(void);
static void shutdown_editor(void);

static void start_search(void);
static void search_key(u32 code, char ch);
static bool find_next(bool from_start);

static void syn_fill_colors(const Line *ln, size_t from, size_t count, u32 *out);
static bool path_is_py(const char *path);
static void update_lang(void);

static void prompt_open(int mode);
static void prompt_key(u32 code, char ch);
static void prompt_exec(void);
static void confirm_yes(void);
static void confirm_no(void);

static void draw_status(u32 cw, u32 rows);
static void draw_prompt(u32 cw, u32 ch_px, u32 rows);
static void draw_help(u32 cw, u32 ch_px);
static void redraw(void);

static void handle_key(u32 code, bool shift, bool ctrl);
static void poll_keyboard(void);
static void te_on_resize(Window *w, s32 old_w, s32 old_h);
static void te_reset_state(void);
static void te_setup(void);
static void TE(void);

/* ===================== утилиты ===================== */

static void *te_memcpy(void *dst, const void *src, size_t n)
{
    char *d = (char *)dst;
    const char *s = (const char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

static void *te_memmove(void *dst, const void *src, size_t n)
{
    char *d = (char *)dst;
    const char *s = (const char *)src;
    if (d == s || n == 0)
        return dst;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

static size_t te_strlen(const char *s)
{
    size_t n = 0;
    while (*s++) n++;
    return n;
}

static void str_copy(char *dst, const char *src)
{
    while ((*dst++ = *src++)) {}
}

static bool str_append(char *dst, size_t cap, const char *src)
{
    size_t l = te_strlen(dst);
    while (l + 1 < cap && *src)
        dst[l++] = *src++;
    dst[l] = 0;
    return *src == 0;
}

static void u32_to_str(u32 v, char *buf)
{
    char t[12];
    size_t n = 0, i;
    do {
        t[n++] = (char)('0' + (v % 10));
        v /= 10;
    } while (v);
    for (i = 0; i < n; i++)
        buf[i] = t[n - 1 - i];
    buf[n] = 0;
}

static void u32_append(char *dst, size_t cap, u32 v)
{
    char t[12];
    u32_to_str(v, t);
    str_append(dst, cap, t);
}

static bool nc_eq(char a, char b)
{
    if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
    if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
    return a == b;
}

static bool is_word_ch(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')
    || (c >= 'a' && c <= 'z') || c == '_';
}

static bool te_memeq(const char *a, const char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (a[i] != b[i])
            return false;
    return true;
}

/* ===================== буфер строк ===================== */

static bool line_reserve(Line *ln, size_t need)
{
    size_t new_capacity;
    char *new_data;

    if (ln->capacity >= need)
        return true;

    new_capacity = ln->capacity ? ln->capacity * 2 : 16;
    while (new_capacity < need)
        new_capacity *= 2;

    new_data = alloc((u32)new_capacity);
    if (!new_data)
        return false;

    if (ln->len)
        te_memcpy(new_data, ln->data, ln->len);
    if (ln->data)
        free(ln->data);

    ln->data = new_data;
    ln->capacity = new_capacity;
    return true;
}

/* собрать строку из до трёх кусков; вся память выделяется заранее */
static bool line_build(Line *out, const char *p1, size_t n1,
                       const char *p2, size_t n2, const char *p3, size_t n3)
{
    size_t total = n1 + n2 + n3;
    char *data = alloc((u32)(total ? total : 1));

    out->data = 0;
    out->len = 0;
    out->capacity = 0;
    if (!data)
        return false;

    if (n1) te_memcpy(data, p1, n1);
    if (n2) te_memcpy(data + n1, p2, n2);
    if (n3) te_memcpy(data + n1 + n2, p3, n3);

    out->data = data;
    out->len = total;
    out->capacity = total ? total : 1;
    return true;
}

static bool line_copy_to(Line *dst, const Line *src)
{
    dst->data = 0;
    dst->len = 0;
    dst->capacity = 0;
    if (src->len) {
        dst->data = alloc((u32)src->len);
        if (!dst->data) {
            dst->len = 0;
            return false;
        }
        te_memcpy(dst->data, src->data, src->len);
        dst->len = src->len;
        dst->capacity = src->len;
    }
    return true;
}

static bool buf_ensure(size_t need)
{
    size_t new_capacity;
    Line *new_lines;

    if (tb.capacity >= need)
        return true;

    new_capacity = tb.capacity ? tb.capacity * 2 : 32;
    while (new_capacity < need)
        new_capacity *= 2;

    new_lines = alloc((u32)(new_capacity * sizeof(Line)));
    if (!new_lines)
        return false;

    if (tb.count)
        te_memcpy(new_lines, tb.lines, tb.count * sizeof(Line));
    if (tb.lines)
        free(tb.lines);

    tb.lines = new_lines;
    tb.capacity = new_capacity;
    return true;
}

static void buf_clear(void)
{
    size_t i;
    for (i = 0; i < tb.count; i++) {
        if (tb.lines[i].data)
            free(tb.lines[i].data);
        tb.lines[i].data = 0;
        tb.lines[i].len = 0;
        tb.lines[i].capacity = 0;
    }
    tb.count = 0;
}

static bool buf_insert_empty_line(size_t row)
{
    if (row > tb.count)
        row = tb.count;
    if (!buf_ensure(tb.count + 1))
        return false;
    te_memmove(&tb.lines[row + 1], &tb.lines[row],
               (tb.count - row) * sizeof(Line));
    tb.lines[row].data = 0;
    tb.lines[row].len = 0;
    tb.lines[row].capacity = 0;
    tb.count++;
    return true;
}

static bool buf_insert_char(size_t row, size_t col, char ch)
{
    Line *ln = &tb.lines[row];
    if (col > ln->len)
        col = ln->len;
    if (!line_reserve(ln, ln->len + 1))
        return false;
    te_memmove(ln->data + col + 1, ln->data + col, ln->len - col);
    ln->data[col] = ch;
    ln->len++;
    return true;
}

static void buf_delete_char(size_t row, size_t col)
{
    Line *ln = &tb.lines[row];
    if (col >= ln->len)
        return;
    te_memmove(ln->data + col, ln->data + col + 1, ln->len - col - 1);
    ln->len--;
}

/* разрыв строки row в позиции col; хвост уходит в строку row+1 */
static bool buf_split_line(size_t row, size_t col)
{
    Line *ln = &tb.lines[row];
    size_t taillen;
    char *tail = 0;

    if (col > ln->len)
        col = ln->len;
    taillen = ln->len - col;

    if (taillen) {
        tail = alloc((u32)taillen);
        if (!tail)
            return false;
        te_memcpy(tail, ln->data + col, taillen);
    }

    if (!buf_insert_empty_line(row + 1)) {
        if (tail)
            free(tail);
        return false;
    }

    if (tail) {
        Line *nl = &tb.lines[row + 1];
        nl->data = tail;
        nl->len = taillen;
        nl->capacity = taillen;
        /* ВАЖНО: tb.lines мог реаллоцироваться внутри buf_insert_empty_line,
         * поэтому длину исходной строки меняем по индексу, а не по указателю. */
        tb.lines[row].len = col;
    }
    return true;
}

/* строка row поглощает строку row+1 */
static bool buf_merge_with_next(size_t row)
{
    Line *a, *b;

    if (row + 1 >= tb.count)
        return true;

    a = &tb.lines[row];
    b = &tb.lines[row + 1];

    if (b->len) {
        char *nd = alloc((u32)(a->len + b->len));
        if (!nd)
            return false;
        if (a->len)
            te_memcpy(nd, a->data, a->len);
        te_memcpy(nd + a->len, b->data, b->len);
        if (a->data)
            free(a->data);
        a->data = nd;
        a->capacity = a->len + b->len;
        a->len += b->len;
    }

    if (b->data)
        free(b->data);
    te_memmove(&tb.lines[row + 1], &tb.lines[row + 2],
               (tb.count - row - 2) * sizeof(Line));
    tb.count--;
    return true;
}

/* вставка текста (может содержать '\n') в (row, col);
 * added — сколько новых строк появилось. Двухфазная. */
static bool buf_insert_text(size_t row, size_t col, const char *s, size_t n,
                            size_t *added)
{
    Line *ln = &tb.lines[row];
    size_t k = 0, i, taillen, seg_i, start;
    char *tail = 0;
    Line *newl;
    bool ok = true;

    if (col > ln->len)
        col = ln->len;

    for (i = 0; i < n; i++)
        if (s[i] == '\n')
            k++;
    *added = k;

    if (k == 0) {
        if (!n)
            return true;
        if (!line_reserve(ln, ln->len + n))
            return false;
        te_memmove(ln->data + col + n, ln->data + col, ln->len - col);
        te_memcpy(ln->data + col, s, n);
        ln->len += n;
        return true;
    }

    taillen = ln->len - col;
    if (taillen) {
        tail = alloc((u32)taillen);
        if (!tail)
            return false;
        te_memcpy(tail, ln->data + col, taillen);
    }

    /* фаза 1: заранее собрать все новые строки */
    newl = alloc((u32)((k + 1) * sizeof(Line)));
    if (!newl) {
        if (tail)
            free(tail);
        return false;
    }

    seg_i = 0;
    start = 0;
    for (i = 0; i <= n && ok; i++) {
        if (i == n || s[i] == '\n') {
            size_t slen = i - start;
            const char *sp = s + start;
            bool res;
            if (seg_i == 0)
                res = line_build(&newl[0], ln->data, col, sp, slen, 0, 0);
            else if (seg_i == k && taillen)
                res = line_build(&newl[seg_i], sp, slen, tail, taillen, 0, 0);
            else
                res = line_build(&newl[seg_i], sp, slen, 0, 0, 0, 0);
            if (!res)
                ok = false;
            seg_i++;
            start = i + 1;
        }
    }

    if (!ok || seg_i != k + 1) {
        size_t j;
        for (j = 0; j < seg_i && j <= k; j++)
            if (newl[j].data)
                free(newl[j].data);
        free(newl);
        if (tail)
            free(tail);
        return false;
    }
    if (tail)
        free(tail);

    if (!buf_ensure(tb.count + k)) {
        size_t j;
        for (j = 0; j <= k; j++)
            if (newl[j].data)
                free(newl[j].data);
        free(newl);
        return false;
    }

    /* фаза 2: без выделений памяти */
    te_memmove(&tb.lines[row + 1 + k], &tb.lines[row + 1],
               (tb.count - row - 1) * sizeof(Line));
    tb.count += k;

    if (tb.lines[row].data)
        free(tb.lines[row].data);
    tb.lines[row] = newl[0];
    for (i = 1; i <= k; i++)
        tb.lines[row + i] = newl[i];
    free(newl);
    return true;
}

/* заменить kill строк в позиции row на глубокие копии src[0..n);
 * двухфазная, при OOM состояние не меняется */
static bool buf_replace(size_t row, size_t kill, const Line *src, size_t n)
{
    Line *tmp;
    size_t i, move_count;
    bool ok = true;

    if (row > tb.count)
        row = tb.count;
    if (kill > tb.count - row)
        kill = tb.count - row;

    tmp = alloc((u32)((n ? n : 1) * sizeof(Line)));
    if (!tmp)
        return false;

    for (i = 0; i < n; i++) {
        if (!line_copy_to(&tmp[i], &src[i])) {
            ok = false;
            break;
        }
    }
    if (!ok) {
        size_t j;
        for (j = 0; j <= i; j++)
            if (tmp[j].data)
                free(tmp[j].data);
        free(tmp);
        return false;
    }

    if (!buf_ensure(tb.count - kill + n)) {
        for (i = 0; i < n; i++)
            if (tmp[i].data)
                free(tmp[i].data);
        free(tmp);
        return false;
    }

    move_count = tb.count - row - kill;
    for (i = 0; i < kill; i++)
        if (tb.lines[row + i].data)
            free(tb.lines[row + i].data);
    te_memmove(&tb.lines[row + n], &tb.lines[row + kill],
               move_count * sizeof(Line));
    for (i = 0; i < n; i++)
        tb.lines[row + i] = tmp[i];
    tb.count = tb.count - kill + n;
    free(tmp);
    return true;
}

/* ===================== undo / redo ===================== */

static void step_free(UndoStep *st)
{
    size_t i;
    if (st->snap) {
        for (i = 0; i < st->snap_count; i++)
            if (st->snap[i].data)
                free(st->snap[i].data);
        free(st->snap);
    }
    st->snap = 0;
    st->snap_count = 0;
}

static void stack_push(UndoStep **arr, size_t *cnt, size_t *cap, UndoStep *st)
{
    if (*cnt == *cap) {
        size_t nc = *cap ? *cap * 2 : 32;
        UndoStep *na = alloc((u32)(nc * sizeof(UndoStep)));
        if (!na) {
            step_free(st);
            return;
        }
        if (*cnt)
            te_memcpy(na, *arr, *cnt * sizeof(UndoStep));
        if (*arr)
            free(*arr);
        *arr = na;
        *cap = nc;
    }
    (*arr)[(*cnt)++] = *st;
}

static void free_undo_all(void)
{
    size_t i;
    for (i = 0; i < ucount; i++)
        step_free(&ustack[i]);
    ucount = 0;
}

static void free_redo_all(void)
{
    size_t i;
    for (i = 0; i < rcount; i++)
        step_free(&rstack[i]);
    rcount = 0;
}

static void undo_stack_trim(void)
{
    if (ucount >= UNDO_LIMIT) {
        step_free(&ustack[0]);
        te_memmove(ustack, ustack + 1, (ucount - 1) * sizeof(UndoStep));
        ucount--;
    }
}

/* снимок n строк с first_row; курсор запоминается для undo */
static bool edit_begin(size_t first_row, size_t n, UndoStep *st)
{
    Line *snap;
    size_t i;

    if (tb.count == 0)
        return false;
    if (first_row >= tb.count)
        first_row = tb.count - 1;
    if (n > tb.count - first_row)
        n = tb.count - first_row;
    if (n == 0)
        return false;

    snap = alloc((u32)(n * sizeof(Line)));
    if (!snap)
        return false;

    for (i = 0; i < n; i++) {
        snap[i].data = 0;
        snap[i].len = 0;
        snap[i].capacity = 0;
    }
    for (i = 0; i < n; i++) {
        if (!line_copy_to(&snap[i], &tb.lines[first_row + i])) {
            size_t j;
            for (j = 0; j <= i; j++)
                if (snap[j].data)
                    free(snap[j].data);
            free(snap);
            return false;
        }
    }

    st->first_row = first_row;
    st->snap_count = n;
    st->snap = snap;
    st->after_count = 0;
    st->cur_row = cur_row;
    st->cur_col = cur_col;
    return true;
}

static void edit_abort(UndoStep *st)
{
    step_free(st);
}

static void edit_commit(UndoStep *st, size_t after_count)
{
    st->after_count = after_count;
    free_redo_all();
    undo_stack_trim();
    stack_push(&ustack, &ucount, &ucap, st);
    modified = true;
    dirty = true;
}

/* глубокая копия n строк с row (для переноса шага между стеками) */
static bool capture_region(size_t row, size_t n, Line **out)
{
    Line *snap;
    size_t i;

    if (row > tb.count)
        row = tb.count;
    if (n > tb.count - row)
        n = tb.count - row;

    snap = alloc((u32)((n ? n : 1) * sizeof(Line)));
    if (!snap)
        return false;

    for (i = 0; i < n; i++) {
        snap[i].data = 0;
        snap[i].len = 0;
        snap[i].capacity = 0;
    }
    for (i = 0; i < n; i++) {
        if (tb.lines[row + i].len) {
            snap[i].data = alloc((u32)tb.lines[row + i].len);
            if (!snap[i].data) {
                size_t j;
                for (j = 0; j <= i; j++)
                    if (snap[j].data)
                        free(snap[j].data);
                free(snap);
                return false;
            }
            te_memcpy(snap[i].data, tb.lines[row + i].data,
                      tb.lines[row + i].len);
            snap[i].len = tb.lines[row + i].len;
            snap[i].capacity = tb.lines[row + i].len;
        }
    }
    *out = snap;
    return true;
}

static void close_group(void)
{
    group_kind = GK_NONE;
}

static bool group_extends(int kind)
{
    return group_kind == kind
    && cur_row == g_row
    && cur_col == g_col
    && (u32)(ticks_now - g_ticks) < GROUP_TIMEOUT;
}

static void restore_cursor_from(UndoStep *st)
{
    cur_row = st->cur_row;
    cur_col = st->cur_col;
    if (tb.count == 0) {
        cur_row = 0;
        cur_col = 0;
    } else {
        if (cur_row >= tb.count)
            cur_row = tb.count - 1;
        if (cur_col > tb.lines[cur_row].len)
            cur_col = tb.lines[cur_row].len;
    }
    sel_active = false;
    follow_cursor();
    dirty = true;
    modified = true;
}

static void do_undo(void)
{
    UndoStep *st, rs;
    size_t cn;
    bool ok;

    close_group();
    if (!ucount) {
        set_msg("Nothing to undo");
        return;
    }

    st = &ustack[ucount - 1];

    /* спасти текущее состояние участка для redo */
    rs.first_row = st->first_row;
    rs.cur_row = cur_row;
    rs.cur_col = cur_col;
    rs.snap = 0;
    cn = st->after_count;
    if (cn > tb.count - rs.first_row)
        cn = tb.count - rs.first_row;
    rs.snap_count = cn;
    rs.after_count = st->snap_count;

    ok = capture_region(rs.first_row, cn, &rs.snap);
    if (ok) {
        stack_push(&rstack, &rcount, &rcap, &rs);
    } else {
        free_redo_all();   /* цепочка redo разорвалась */
    }

    if (!buf_replace(st->first_row, cn, st->snap, st->snap_count)) {
        set_msg(MSG_NOMEM);
        return;
    }
    restore_cursor_from(st);

    ucount--;
    step_free(&ustack[ucount]);
}

static void do_redo(void)
{
    UndoStep *st, us;
    size_t cn;
    bool ok;

    close_group();
    if (!rcount) {
        set_msg("Nothing to redo");
        return;
    }

    st = &rstack[rcount - 1];

    /* спасти текущее состояние участка для возможного undo */
    us.first_row = st->first_row;
    us.cur_row = cur_row;
    us.cur_col = cur_col;
    us.snap = 0;
    cn = st->after_count;
    if (cn > tb.count - us.first_row)
        cn = tb.count - us.first_row;
    us.snap_count = cn;
    us.after_count = st->snap_count;

    ok = capture_region(us.first_row, cn, &us.snap);
    if (ok) {
        undo_stack_trim();
        stack_push(&ustack, &ucount, &ucap, &us);
    } else {
        set_msg(MSG_NOMEM);
    }

    if (!buf_replace(st->first_row, cn, st->snap, st->snap_count)) {
        set_msg(MSG_NOMEM);
        return;
    }
    restore_cursor_from(st);

    rcount--;
    step_free(&rstack[rcount]);
}

/* ===================== выделение и буфер обмена ===================== */

static void sel_norm(size_t *r1, size_t *c1, size_t *r2, size_t *c2)
{
    if (!sel_active) {
        *r1 = *r2 = cur_row;
        *c1 = *c2 = cur_col;
        return;
    }
    if (sel_row < cur_row || (sel_row == cur_row && sel_col <= cur_col)) {
        *r1 = sel_row;  *c1 = sel_col;
        *r2 = cur_row;  *c2 = cur_col;
    } else {
        *r1 = cur_row;  *c1 = cur_col;
        *r2 = sel_row;  *c2 = sel_col;
    }
}

static bool in_sel(size_t row, size_t col)
{
    size_t r1, c1, r2, c2;
    sel_norm(&r1, &c1, &r2, &c2);
    if (r1 == r2)
        return row == r1 && col >= c1 && col < c2;
    if (row == r1)
        return col >= c1;
    if (row == r2)
        return col < c2;
    return row > r1 && row < r2;
}

/* удалить диапазон (r1,c1)..(r2,c2); курсор в (r1,c1) */
static bool edit_delete_range(size_t r1, size_t c1, size_t r2, size_t c2)
{
    UndoStep st;

    close_group();
    if (!edit_begin(r1, r2 - r1 + 1, &st)) {
        set_msg(MSG_NOMEM);
        return false;
    }

    if (r1 == r2) {
        Line *ln = &tb.lines[r1];
        if (c2 > ln->len)
            c2 = ln->len;
        if (c1 > c2)
            c1 = c2;
        te_memmove(ln->data + c1, ln->data + c2, ln->len - c2);
        ln->len -= (c2 - c1);
    } else {
        Line *a = &tb.lines[r1];
        Line *b = &tb.lines[r2];
        Line merged;
        size_t i;

        if (!line_build(&merged, a->data, c1, b->data + c2, b->len - c2, 0, 0)) {
            edit_abort(&st);
            set_msg(MSG_NOMEM);
            return false;
        }
        if (a->data)
            free(a->data);
        a->data = merged.data;
        a->len = merged.len;
        a->capacity = merged.capacity;

        for (i = r1 + 1; i <= r2; i++)
            if (tb.lines[i].data)
                free(tb.lines[i].data);
        te_memmove(&tb.lines[r1 + 1], &tb.lines[r2 + 1],
                   (tb.count - r2 - 1) * sizeof(Line));
        tb.count -= (r2 - r1);
    }

    edit_commit(&st, (r1 == r2) ? 1 : (r2 - r1));
    cur_row = r1;
    cur_col = c1;
    sel_active = false;
    follow_cursor();
    dirty = true;
    modified = true;
    return true;
}

static bool delete_selection(void)
{
    size_t r1, c1, r2, c2;
    bool res;

    if (!sel_active)
        return false;
    sel_norm(&r1, &c1, &r2, &c2);
    sel_active = false;
    if (r1 == r2 && c1 == c2)
        return false;
    res = edit_delete_range(r1, c1, r2, c2);
    return res;
}

/* копия выделения или текущей строки в буфер обмена;
 * cut=true — ещё и удалить */
static void do_copy(bool cut)
{
    size_t r1, c1, r2, c2, r, total = 0, off = 0;
    char *buf;
    bool lines_mode = true;

    if (sel_active) {
        sel_norm(&r1, &c1, &r2, &c2);
        if (!(r1 == r2 && c1 == c2))
            lines_mode = false;
    }
    if (lines_mode) {
        r1 = r2 = cur_row;
        c1 = 0;
        c2 = tb.lines[cur_row].len;
    }

    if (lines_mode) {
        for (r = r1; r <= r2; r++)
            total += tb.lines[r].len + 1;
    } else if (r1 == r2) {
        total = c2 - c1;
    } else {
        total += tb.lines[r1].len - c1 + 1;
        for (r = r1 + 1; r < r2; r++)
            total += tb.lines[r].len + 1;
        total += c2;
    }

    if (!total) {
        set_msg("Nothing to copy");
        return;
    }

    buf = alloc((u32)total);
    if (!buf) {
        set_msg(MSG_NOMEM);
        return;
    }

    if (lines_mode) {
        for (r = r1; r <= r2; r++) {
            Line *ln = &tb.lines[r];
            if (ln->len) {
                te_memcpy(buf + off, ln->data, ln->len);
                off += ln->len;
            }
            buf[off++] = '\n';
        }
    } else if (r1 == r2) {
        te_memcpy(buf, tb.lines[r1].data + c1, c2 - c1);
        off = c2 - c1;
    } else {
        Line *a = &tb.lines[r1];
        te_memcpy(buf + off, a->data + c1, a->len - c1);
        off += a->len - c1;
        buf[off++] = '\n';
        for (r = r1 + 1; r < r2; r++) {
            Line *ln = &tb.lines[r];
            if (ln->len) {
                te_memcpy(buf + off, ln->data, ln->len);
                off += ln->len;
            }
            buf[off++] = '\n';
        }
        if (c2) {
            te_memcpy(buf + off, tb.lines[r2].data, c2);
            off += c2;
        }
    }

    if (clip)
        free(clip);
    clip = buf;
    clip_len = total;

    if (cut) {
        if (lines_mode) {
            if (r1 + 1 < tb.count)
                edit_delete_range(r1, 0, r1 + 1, 0);
            else
                edit_delete_range(r1, 0, r1, tb.lines[r1].len);
        } else {
            edit_delete_range(r1, c1, r2, c2);
        }
        set_msg("Cut");
    } else {
        set_msg("Copied");
    }
}

static void do_paste(void)
{
    UndoStep st;
    size_t added = 0, r, c, i;
    bool ok;

    if (!clip || !clip_len) {
        set_msg("Clipboard is empty");
        return;
    }
    if (sel_active)
        delete_selection();

    close_group();
    if (!edit_begin(cur_row, 1, &st)) {
        set_msg(MSG_NOMEM);
        return;
    }
    ok = buf_insert_text(cur_row, cur_col, clip, clip_len, &added);
    if (!ok) {
        edit_abort(&st);
        set_msg(MSG_NOMEM);
        return;
    }
    edit_commit(&st, 1 + added);

    r = cur_row;
    c = cur_col;
    for (i = 0; i < clip_len; i++) {
        if (clip[i] == '\n') {
            r++;
            c = 0;
        } else {
            c++;
        }
    }
    cur_row = r;
    cur_col = c;
    follow_cursor();
    dirty = true;
    modified = true;
}

/* ===================== курсор и прокрутка ===================== */

static size_t win_cols(void)
{
    if (!TextEditor || TextEditor->width < 2 * BORDER + FONT_W)
        return 0;
    return (size_t)(TextEditor->width - 2 * BORDER) / CELL_W;
}

static size_t win_rows(void)
{
    if (!TextEditor || TextEditor->height < TITLE_H + BORDER + FONT_H)
        return 0;
    return (size_t)(TextEditor->height - TITLE_H - BORDER) / FONT_H;
}

static size_t text_rows(void)
{
    size_t r = win_rows();
    return r ? r - 1 : 0;   /* нижняя строка — статус */
}

static size_t gutter_width(void)
{
    size_t d = 1, v = tb.count;
    while (v >= 10) {
        v /= 10;
        d++;
    }
    return d + 1 > 2 ? d + 1 : 2;
}

static size_t text_cols(void)
{
    size_t wc = win_cols();
    size_t g = gutter_width();
    if (wc <= g)
        return 0;
    return wc - g;
}

static void follow_cursor(void)
{
    size_t rows = text_rows();
    size_t cols = text_cols();

    if (rows == 0 || cols == 0)
        return;
    if (cur_row < scroll_top)
        scroll_top = cur_row;
    if (cur_row >= scroll_top + rows)
        scroll_top = cur_row + 1 - rows;
    if (cur_col < scroll_left)
        scroll_left = cur_col;
    if (cur_col >= scroll_left + cols)
        scroll_left = cur_col + 1 - cols;
}

static void cursor_to(size_t row, size_t col, bool select)
{
    if (tb.count == 0) {
        row = 0;
        col = 0;
    } else {
        if (row >= tb.count)
            row = tb.count - 1;
        if (col > tb.lines[row].len)
            col = tb.lines[row].len;
    }
    if (select) {
        if (!sel_active) {
            sel_active = true;
            sel_row = cur_row;
            sel_col = cur_col;
        }
    } else {
        sel_active = false;
    }
    cur_row = row;
    cur_col = col;
    close_group();
    follow_cursor();
    dirty = true;
}

static void move_left(bool select)
{
    if (cur_col) {
        cur_col--;
    } else if (cur_row) {
        cur_row--;
        cur_col = tb.lines[cur_row].len;
    }
    cursor_to(cur_row, cur_col, select);
}

static void move_right(bool select)
{
    if (cur_row < tb.count && cur_col < tb.lines[cur_row].len) {
        cur_col++;
    } else if (cur_row + 1 < tb.count) {
        cur_row++;
        cur_col = 0;
    }
    cursor_to(cur_row, cur_col, select);
}

static void move_up(bool select)
{
    if (cur_row)
        cursor_to(cur_row - 1, cur_col, select);
}

static void move_down(bool select)
{
    if (cur_row + 1 < tb.count)
        cursor_to(cur_row + 1, cur_col, select);
}

static void move_word_right(bool select)
{
    size_t r = cur_row, c = cur_col;

    while (r < tb.count) {
        Line *ln = &tb.lines[r];
        while (c < ln->len && !is_word_ch(ln->data[c]))
            c++;
        if (c < ln->len) {
            while (c < ln->len && is_word_ch(ln->data[c]))
                c++;
            break;
        }
        r++;
        c = 0;
    }
    if (r >= tb.count) {
        r = tb.count - 1;
        c = tb.lines[r].len;
    }
    cursor_to(r, c, select);
}

static void move_word_left(bool select)
{
    size_t r = cur_row, c = cur_col;

    while (1) {
        Line *ln = &tb.lines[r];
        while (c > 0 && !is_word_ch(ln->data[c - 1]))
            c--;
        if (c > 0) {
            while (c > 0 && is_word_ch(ln->data[c - 1]))
                c--;
            break;
        }
        if (r == 0)
            break;
        r--;
        c = tb.lines[r].len;
    }
    cursor_to(r, c, select);
}

static void move_page(bool up, bool select)
{
    size_t rows = text_rows();
    size_t step = rows > 1 ? rows - 1 : 1;
    size_t nr;

    if (up)
        nr = cur_row > step ? cur_row - step : 0;
    else
        nr = cur_row + step < tb.count ? cur_row + step : tb.count - 1;
    cursor_to(nr, cur_col, select);
}

/* ===================== правка ===================== */

static void do_insert_char(char ch)
{
    UndoStep st;

    if (sel_active)
        delete_selection();

    if (group_extends(GK_INS)) {
        if (!buf_insert_char(cur_row, cur_col, ch)) {
            set_msg(MSG_NOMEM);
            return;
        }
        cur_col++;
        g_col = cur_col;
        g_ticks = ticks_now;
        follow_cursor();
        dirty = true;
        modified = true;
        return;
    }

    close_group();
    if (!edit_begin(cur_row, 1, &st)) {
        set_msg(MSG_NOMEM);
        return;
    }
    if (!buf_insert_char(cur_row, cur_col, ch)) {
        edit_abort(&st);
        set_msg(MSG_NOMEM);
        return;
    }
    edit_commit(&st, 1);
    group_kind = GK_INS;
    g_row = cur_row;
    g_col = cur_col + 1;
    g_ticks = ticks_now;
    cur_col++;
    follow_cursor();
    dirty = true;
    modified = true;
}

static void do_type_char(char ch)
{
    UndoStep st;

    if (sel_active)
        delete_selection();

    if (ovr && cur_col < tb.lines[cur_row].len) {
        close_group();
        if (!edit_begin(cur_row, 1, &st)) {
            set_msg(MSG_NOMEM);
            return;
        }
        tb.lines[cur_row].data[cur_col] = ch;
        edit_commit(&st, 1);
        cur_col++;
        follow_cursor();
        dirty = true;
        modified = true;
        return;
    }
    do_insert_char(ch);
}

static void do_backspace(void)
{
    UndoStep st;
    size_t prev_len;

    if (sel_active) {
        delete_selection();
        return;
    }

    if (cur_col > 0) {
        if (group_extends(GK_BS)) {
            buf_delete_char(cur_row, cur_col - 1);
            cur_col--;
            g_col = cur_col;
            g_ticks = ticks_now;
            dirty = true;
            modified = true;
            return;
        }
        close_group();
        if (!edit_begin(cur_row, 1, &st)) {
            set_msg(MSG_NOMEM);
            return;
        }
        buf_delete_char(cur_row, cur_col - 1);
        edit_commit(&st, 1);
        group_kind = GK_BS;
        g_row = cur_row;
        g_col = cur_col - 1;
        g_ticks = ticks_now;
        cur_col--;
        dirty = true;
        modified = true;
        return;
    }

    if (cur_row == 0)
        return;

    /* слияние с предыдущей строкой */
    close_group();
    prev_len = tb.lines[cur_row - 1].len;
    if (!edit_begin(cur_row - 1, 2, &st)) {
        set_msg(MSG_NOMEM);
        return;
    }
    if (!buf_merge_with_next(cur_row - 1)) {
        edit_abort(&st);
        set_msg(MSG_NOMEM);
        return;
    }
    edit_commit(&st, 1);
    cur_row--;
    cur_col = prev_len;
    follow_cursor();
    dirty = true;
    modified = true;
}

static void do_delete(void)
{
    UndoStep st;

    if (sel_active) {
        delete_selection();
        return;
    }

    if (tb.count && cur_col < tb.lines[cur_row].len) {
        close_group();
        if (!edit_begin(cur_row, 1, &st)) {
            set_msg(MSG_NOMEM);
            return;
        }
        buf_delete_char(cur_row, cur_col);
        edit_commit(&st, 1);
        return;
    }

    if (cur_row + 1 < tb.count) {
        close_group();
        if (!edit_begin(cur_row, 2, &st)) {
            set_msg(MSG_NOMEM);
            return;
        }
        if (!buf_merge_with_next(cur_row)) {
            edit_abort(&st);
            set_msg(MSG_NOMEM);
            return;
        }
        edit_commit(&st, 1);
    }
}

static void do_enter(void)
{
    UndoStep st;

    if (sel_active)
        delete_selection();

    close_group();
    if (!edit_begin(cur_row, 1, &st)) {
        set_msg(MSG_NOMEM);
        return;
    }
    if (!buf_split_line(cur_row, cur_col)) {
        edit_abort(&st);
        set_msg(MSG_NOMEM);
        return;
    }
    edit_commit(&st, 2);
    cur_row++;
    cur_col = 0;
    follow_cursor();
    dirty = true;
    modified = true;
}

static void do_tab(void)
{
    size_t n = TAB_WIDTH - (cur_col % TAB_WIDTH);
    while (n--)
        do_insert_char(' ');
}

/* ===================== сообщения ===================== */

static void set_msg(const char *s)
{
    str_copy(status_msg, s);
    msg_until = ticks_now + MSG_TIME;
}

/* ===================== файлы ===================== */

static void load_from(struct file *f)
{
    u32 size, bytes, i;
    char *data;
    size_t row = 0;

    if (!f || !f->read || !f->getsize) {
        set_msg("Bad file handle");
        return;
    }

    buf_clear();
    tb.count = 0;
    if (!buf_insert_empty_line(0)) {
        set_msg(MSG_NOMEM);
        cur_row = cur_col = 0;
        scroll_top = scroll_left = 0;
        dirty = true;
        return;
    }
    cur_row = cur_col = 0;
    scroll_top = scroll_left = 0;
    sel_active = false;
    close_group();
    free_undo_all();
    free_redo_all();

    size = f->getsize(f);
    if (size == 0) {
        modified = false;
        dirty = true;
        return;
    }

    if ((i_ptr)size + 1024 > getfree()) {
        set_msg("File too large");
        modified = false;
        dirty = true;
        return;
    }

    data = alloc(size);
    if (!data) {
        set_msg(MSG_NOMEM);
        modified = false;
        dirty = true;
        return;
    }

    bytes = f->read(f, data, size, 0);
    for (i = 0; i < bytes; i++) {
        char ch = data[i];
        if (ch == '\n') {
            if (!buf_insert_empty_line(row + 1)) {
                set_msg("Low memory: file truncated");
                break;
            }
            row++;
        } else if (ch != '\r') {
            Line *ln = &tb.lines[row];
            if (!line_reserve(ln, ln->len + 1)) {
                set_msg("Low memory: file truncated");
                break;
            }
            ln->data[ln->len++] = ch;
        }
    }
    free(data);

    cur_row = 0;
    cur_col = 0;
    modified = false;
    follow_cursor();
    dirty = true;
}

static bool save_current(void)
{
    u32 total = 0, off = 0, r;
    char *data;
    bool ok = true;

    if (!has_path) {
        start_saveas();
        return false;
    }

    if (!g_file) {
        if (g_disk) {
            g_file = open(g_disk, file_path);
            if (!g_file)
                g_file = create_file(g_disk, file_path);
        }
        if (!g_file) {
            set_msg("Save failed");
            return false;
        }
    }
    if (!g_file->write) {
        set_msg("Save failed");
        return false;
    }

    for (r = 0; r < tb.count; r++) {
        total += (u32)tb.lines[r].len;
        if (r + 1 < tb.count)
            total++;
    }

    data = alloc(total ? total : 1);
    if (!data) {
        set_msg(MSG_NOMEM);
        return false;
    }

    for (r = 0; r < tb.count; r++) {
        Line *ln = &tb.lines[r];
        if (ln->len) {
            te_memcpy(data + off, ln->data, ln->len);
            off += (u32)ln->len;
        }
        if (r + 1 < tb.count)
            data[off++] = '\n';
    }

    /* total == 0 не пишем: пустой буфер не должен стирать файл целиком */
    if (total > 0 && g_file->write(g_file, data, total, 0) != total)
        ok = false;
    free(data);

    if (ok) {
        modified = false;
        set_msg("Saved");
        dirty = true;
    } else {
        set_msg("Write failed");
    }
    return ok;
}

static void do_open_path(const char *path)
{
    struct file *f = g_disk ? open(g_disk, (char *)path) : 0;

    if (!f) {
        set_msg("Open failed");
        return;
    }
    g_file = f;
    str_copy(file_path, path);
    has_path = true;
    load_from(f);
    update_lang();
    set_msg("Opened");
}

static void do_saveas_path(const char *path)
{
    struct file *f = 0;

    if (g_disk) {
        f = open(g_disk, (char *)path);
        if (!f)
            f = create_file(g_disk, (char *)path);
    }
    if (!f) {
        set_msg("Cannot create file");
        return;
    }
    g_file = f;
    str_copy(file_path, path);
    has_path = true;
    update_lang();
    save_current();
}

static void start_open(void)
{
    if (modified) {
        confirm_action = CA_OPEN;
        prompt_open(PM_CONFIRM);
    } else {
        prompt_open(PM_OPEN);
    }
}

static void start_saveas(void)
{
    prompt_open(PM_SAVEAS);
}

static void start_quit(void)
{
    if (modified) {
        confirm_action = CA_QUIT;
        prompt_open(PM_CONFIRM);
    } else {
        shutdown_editor();
    }
}

static void shutdown_editor(void)
{
    close_group();
    free_undo_all();
    free_redo_all();
    buf_clear();
    tb.count = 0;
    if (clip) {
        free(clip);
        clip = 0;
        clip_len = 0;
    }
    sel_active = false;
    closed = true;
    dirty = true;
}

/* ===================== поиск ===================== */

static bool line_match_nc(Line *ln, size_t pos)
{
    size_t i;
    if (ln->len - pos < query_len)
        return false;
    for (i = 0; i < query_len; i++)
        if (!nc_eq(ln->data[pos + i], query[i]))
            return false;
    return true;
}

static bool find_next(bool from_start)
{
    size_t r, c;
    bool wrapped = false;

    if (!query_len) {
        set_msg("No search text");
        return false;
    }
    have_query = true;

    if (from_start || !tb.count) {
        r = 0;
        c = 0;
    } else if (sel_active) {
        /* F3: продолжить после конца текущего совпадения */
        size_t r1, c1, r2, c2;
        sel_norm(&r1, &c1, &r2, &c2);
        r = r2;
        c = c2;
    } else {
        /* первый поиск: совпадение под курсором тоже считается */
        r = cur_row;
        c = cur_col;
    }

    while (1) {
        for (; r < tb.count; r++, c = 0) {
            Line *ln = &tb.lines[r];
            for (; c + query_len <= ln->len; c++) {
                if (line_match_nc(ln, c)) {
                    sel_active = true;
                    sel_row = r;
                    sel_col = c;
                    cur_row = r;
                    cur_col = c + query_len;
                    follow_cursor();
                    dirty = true;
                    return true;
                }
            }
        }
        if (wrapped || !tb.count)
            break;
        wrapped = true;
        r = 0;
        c = 0;
    }

    set_msg("Not found");
    return false;
}

static void start_search(void)
{
    size_t r1, c1, r2, c2;

    search_mode = true;
    query_len = 0;
    query[0] = 0;

    /* подставить текущее выделение в пределах одной строки */
    if (sel_active) {
        sel_norm(&r1, &c1, &r2, &c2);
        if (r1 == r2 && c2 > c1) {
            size_t n = c2 - c1;
            if (n > MAX_QUERY)
                n = MAX_QUERY;
            te_memcpy(query, tb.lines[r1].data + c1, n);
            query[n] = 0;
            query_len = n;
        }
    }
    dirty = true;
}

static void search_key(u32 code, char ch)
{
    if (code == SC_ESC) {
        search_mode = false;
        dirty = true;
        return;
    }
    if (code == SC_ENTER) {
        search_mode = false;
        find_next(false);
        return;
    }
    if (code == SC_BSP) {
        if (query_len) {
            query_len--;
            query[query_len] = 0;
        }
        dirty = true;
        return;
    }
    if (ch >= 32 && ch <= 126 && query_len < MAX_QUERY) {
        query[query_len++] = ch;
        query[query_len] = 0;
        dirty = true;
    }
}

/* ===================== диалоги ===================== */

static void prompt_open(int mode)
{
    prompt_mode = mode;
    plen = 0;
    pbuf[0] = 0;
    if (mode == PM_SAVEAS && has_path) {
        str_copy(pbuf, file_path);
        plen = te_strlen(pbuf);
    }
    dirty = true;
}

static void prompt_exec(void)
{
    pbuf[plen] = 0;

    if (prompt_mode == PM_OPEN) {
        do_open_path(pbuf);
    } else if (prompt_mode == PM_SAVEAS) {
        do_saveas_path(pbuf);
    } else if (prompt_mode == PM_GOTO) {
        u32 n = 0;
        size_t i;
        for (i = 0; i < plen; i++) {
            if (pbuf[i] < '0' || pbuf[i] > '9') {
                set_msg("Line number required");
                return;
            }
            n = n * 10 + (u32)(pbuf[i] - '0');
            if (n > 1000000)
                n = 1000000;
        }
        if (!n) {
            set_msg("Line number required");
            return;
        }
        cursor_to(n - 1, 0, false);
    }
}

static void prompt_key(u32 code, char ch)
{
    if (prompt_mode == PM_CONFIRM) {
        if (code == SC_ESC) {
            prompt_mode = PM_NONE;
            dirty = true;
            return;
        }
        if (code == SC_ENTER || keyboard_map[code] == 'y') {
            prompt_mode = PM_NONE;
            confirm_yes();
            return;
        }
        if (keyboard_map[code] == 'n') {
            prompt_mode = PM_NONE;
            confirm_no();
            return;
        }
        return;
    }

    if (code == SC_ESC) {
        prompt_mode = PM_NONE;
        dirty = true;
        return;
    }
    if (code == SC_ENTER) {
        prompt_mode = PM_NONE;
        prompt_exec();
        dirty = true;
        return;
    }
    if (code == SC_BSP) {
        if (plen) {
            plen--;
            pbuf[plen] = 0;
        }
        dirty = true;
        return;
    }
    if (prompt_mode == PM_GOTO) {
        if (ch >= '0' && ch <= '9' && plen < 8) {
            pbuf[plen++] = ch;
            pbuf[plen] = 0;
        }
        dirty = true;
        return;
    }
    if (ch >= 32 && ch <= 126 && plen < MAX_PATH) {
        pbuf[plen++] = ch;
        pbuf[plen] = 0;
        dirty = true;
    }
}

static void confirm_yes(void)
{
    if (confirm_action == CA_OPEN) {
        save_current();
        prompt_open(PM_OPEN);
    } else {
        save_current();
        shutdown_editor();
    }
}

static void confirm_no(void)
{
    if (confirm_action == CA_OPEN) {
        prompt_open(PM_OPEN);
    } else {
        shutdown_editor();
    }
}

/* ===================== подсветка синтаксиса ===================== */

/* Палитра и словари взяты из подсветчика _pfs (cli_NS.c). Язык выбирается
 * автоматически: по расширению .py, иначе по характерным словам буфера
 * (как detect_lang); по умолчанию ZhirScript — язык ОС. Разбор построчный
 * и без межстрочного состояния, как в _pfs: незакрытая строка длится до
 * конца строки, '#' — комментарий в обоих языках, '//' — комментарий
 * только в ZhirScript (в Python это оператор деления). */

static const char *syn_py_keywords[] = {
    "def","return","if","elif","else","for","while","in","import","from",
    "as","class","pass","break","continue","lambda","yield","with","try",
    "except","finally","raise","global","del","not","and","or","is",
    "None","True","False", 0
};

static const char *syn_py_builtins[] = {
    "print","range","len","int","float","str","chr","ord","sum","abs",
    "min","max","enumerate","zip","list","dict","set","tuple","type",
    "bool","input","open", 0
};

static const char *syn_zs_keywords[] = {
    "udf","ret","var","whl","if","else","and","or","not",
    "brk","cnt","loop", 0
};

static const char *syn_zs_types[] = {
    "u8","u16","u32","u64","i8","i16","i32","i64",
    "fp","f32","f64","str","bool","void", 0
};

static const char *syn_zs_builtins[] = {
    "println","printf","len","type","ord","chr","int","float","str", 0
};

static bool syn_is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool syn_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool syn_is_ident(char c)
{
    return syn_is_alpha(c) || syn_is_digit(c);
}

static bool syn_is_op(char c)
{
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '='
    || c == '<' || c == '>' || c == '!' || c == '&' || c == '|'
    || c == '^' || c == '~' || c == '%';
}

static bool syn_is_punct(char c)
{
    return c == '(' || c == ')' || c == '[' || c == ']' || c == '{'
    || c == '}' || c == ',' || c == ';' || c == '.' || c == ':'
    || c == '?';
}

static bool syn_word_in_list(const char *s, size_t len, const char **list)
{
    size_t i;
    for (i = 0; list[i]; i++)
        if (te_strlen(list[i]) == len && te_memeq(s, list[i], len))
            return true;
    return false;
}

/* цвет каждого символа строки в колонках [from, from+count) */
static void syn_fill_colors(const Line *ln, size_t from, size_t count, u32 *out)
{
    const char *p = ln->data;
    size_t len = ln->len;
    size_t pos = 0, i;

    for (i = 0; i < count; i++)
        out[i] = C_FG;

    while (pos < len) {
        char c = p[pos];
        size_t start = pos;
        u32 col = C_FG;

        if (c == ' ' || c == '\t') {
            pos++;
            continue;
        }

        if (c == '#'
            || (syn_lang == SYN_ZHIR && c == '/'
            && pos + 1 < len && p[pos + 1] == '/')) {
            /* комментарий до конца строки */
            col = SY_COM;
        pos = len;
            } else if (c == '"' || c == '\'') {
                char q = c;
                col = SY_STR;
                pos++;
                while (pos < len) {
                    if (p[pos] == '\\' && pos + 1 < len) {
                        pos += 2;   /* экранированный символ */
                        continue;
                    }
                    if (p[pos] == q) {
                        pos++;
                        break;
                    }
                    pos++;
                }
            } else if (syn_is_digit(c)
                || (c == '.' && pos + 1 < len && syn_is_digit(p[pos + 1]))) {
                col = SY_NUM;
            while (pos < len && (syn_is_ident(p[pos]) || p[pos] == '.'))
                pos++;
                } else if (syn_is_alpha(c)) {
                    size_t wl;
                    while (pos < len && syn_is_ident(p[pos]))
                        pos++;
                    wl = pos - start;
                    if (syn_lang == SYN_PY) {
                        if (syn_word_in_list(p + start, wl, syn_py_keywords))
                            col = SY_KW;
                        else if (syn_word_in_list(p + start, wl, syn_py_builtins))
                            col = SY_BUILT;
                    } else {
                        if (syn_word_in_list(p + start, wl, syn_zs_keywords))
                            col = SY_KW;
                        else if (syn_word_in_list(p + start, wl, syn_zs_types))
                            col = SY_TYPE;
                        else if (syn_word_in_list(p + start, wl, syn_zs_builtins))
                            col = SY_BUILT;
                    }
                    if (col == C_FG) {
                        /* обычный идентификатор, за которым стоит '(' — функция */
                        size_t q2 = pos;
                        while (q2 < len && (p[q2] == ' ' || p[q2] == '\t'))
                            q2++;
                        if (q2 < len && p[q2] == '(')
                            col = SY_FUNC;
                    }
                } else if (syn_is_op(c)) {
                    col = SY_OP;
                    while (pos < len && syn_is_op(p[pos]))
                        pos++;
                } else if (syn_is_punct(c)) {
                    col = SY_PUNCT;
                    pos++;
                } else {
                    pos++;
                }

                /* покрасить пересечение [start, pos) с окном [from, from+count) */
                if (pos > from && start < from + count) {
                    size_t b = start > from ? start : from;
                    size_t e = pos < from + count ? pos : from + count;
                    for (; b < e; b++)
                        out[b - from] = col;
                }
    }
}

/* вхождение подстроки в строку буфера (как strstr, но по длине) */
static bool syn_line_contains(const Line *ln, const char *needle)
{
    size_t nl = te_strlen(needle);
    size_t i;

    if (!nl || ln->len < nl)
        return false;
    for (i = 0; i + nl <= ln->len; i++)
        if (te_memeq(ln->data + i, needle, nl))
            return true;
    return false;
}

static bool path_is_py(const char *path)
{
    size_t n = te_strlen(path);
    return n >= 3 && nc_eq(path[n - 3], '.')
    && nc_eq(path[n - 2], 'p') && nc_eq(path[n - 1], 'y');
}

/* определение языка по характерным словам (как detect_lang в _pfs);
 * просматривается не больше SYN_SCAN_ROWS строк, чтобы не тормозить */
static int syn_detect_lang(void)
{
    static const char *zhir_marks[] = {
        "udf ", "whl ", "var u32", "var fp", "println(", 0
    };
    static const char *py_marks[] = {
        "def ", "elif", "import ", "self", "__", 0
    };
    size_t r, i, lim;

    lim = tb.count < SYN_SCAN_ROWS ? tb.count : SYN_SCAN_ROWS;
    for (r = 0; r < lim; r++)
        for (i = 0; zhir_marks[i]; i++)
            if (syn_line_contains(&tb.lines[r], zhir_marks[i]))
                return SYN_ZHIR;
    for (r = 0; r < lim; r++)
        for (i = 0; py_marks[i]; i++)
            if (syn_line_contains(&tb.lines[r], py_marks[i]))
                return SYN_PY;
    return SYN_ZHIR;
}

/* язык текущего файла: сначала расширение .py, затем содержимое буфера */
static void update_lang(void)
{
    if (has_path && path_is_py(file_path))
        syn_lang = SYN_PY;
    else
        syn_lang = syn_detect_lang();
}

/* ===================== отрисовка ===================== */

static void draw_status(u32 cw, u32 rows)
{
    u32 y = TITLE_H + rows * FONT_H;
    char left[128];
    char right[64];
    size_t lw, rw, maxc, keep;
    u32 sbg = search_mode ? C_PANEL : C_STATUS_BG;
    u32 sfg = search_mode ? C_FG : C_STATUS_FG;

    window_fill_rect(TextEditor, BORDER, y, cw, FONT_H, sbg);

    left[0] = 0;
    right[0] = 0;

    if (search_mode) {
        str_copy(left, "Find: ");
        str_append(left, sizeof(left), query);
        str_append(left, sizeof(left), "_  Enter next / Esc");
    } else if (ticks_now < msg_until && status_msg[0]) {
        str_copy(left, status_msg);
    } else {
        str_copy(left, has_path ? file_path : "Untitled");
        if (modified)
            str_append(left, sizeof(left), " *");
    }

    str_copy(right, "Ln ");
    u32_append(right, sizeof(right), (u32)(cur_row + 1));
    str_append(right, sizeof(right), ", Col ");
    u32_append(right, sizeof(right), (u32)(cur_col + 1));
    str_append(right, sizeof(right), "  ");
    u32_append(right, sizeof(right), (u32)tb.count);
    str_append(right, sizeof(right), " ln  ");
    str_append(right, sizeof(right), ovr ? "OVR" : "INS");

    maxc = cw / CELL_W;
    lw = te_strlen(left);
    rw = te_strlen(right);

    if (lw + rw + 2 > maxc && maxc > rw + 4) {
        keep = maxc - rw - 4;
        draw_string(TextEditor, "..", BORDER + CELL_W, y, sfg);
        draw_string(TextEditor, left + (lw - keep),
                    BORDER + 3 * CELL_W, y, sfg);
    } else {
        draw_string(TextEditor, left, BORDER + CELL_W, y, sfg);
    }
    if ((u32)rw * CELL_W + 2 * CELL_W <= cw)
        draw_string(TextEditor, right,
                    BORDER + cw - (u32)rw * CELL_W - CELL_W, y, sfg);
}

static void draw_prompt(u32 cw, u32 ch_px, u32 rows)
{
    u32 bw, bh, bx, by;
    const char *title = "";
    const char *hint = "Enter: OK   Esc: cancel";
    u32 iy;

    (void)rows;

    if (cw < 12 * CELL_W || ch_px < 6 * FONT_H)
        return;

    bw = 48 * CELL_W;
    if (bw + 4 * CELL_W > cw)
        bw = cw - 4 * CELL_W;
    bh = 5 * FONT_H;
    bx = BORDER + (cw - bw) / 2;
    by = TITLE_H + (ch_px - bh) / 2;

    if (prompt_mode == PM_CONFIRM) {
        title = "Save changes?";
        hint = "Y: yes   N: no   Esc: cancel";
    } else if (prompt_mode == PM_OPEN) {
        title = "Open file";
    } else if (prompt_mode == PM_SAVEAS) {
        title = "Save as";
    } else if (prompt_mode == PM_GOTO) {
        title = "Go to line";
    }

    window_fill_rect(TextEditor, bx + 3, by + 3, bw, bh, C_SHADOW);
    window_fill_rect(TextEditor, bx, by, bw, bh, C_PANEL);
    window_fill_rect(TextEditor, bx, by, bw, 1, C_PANEL_BR);
    window_fill_rect(TextEditor, bx, by + bh - 1, bw, 1, C_PANEL_BR);
    window_fill_rect(TextEditor, bx, by, 1, bh, C_PANEL_BR);
    window_fill_rect(TextEditor, bx + bw - 1, by, 1, bh, C_PANEL_BR);

    draw_string(TextEditor, title, bx + CELL_W, by + 6, C_STATUS_FG);

    if (prompt_mode == PM_CONFIRM) {
        draw_string(TextEditor, hint, bx + CELL_W,
                    by + bh - FONT_H - 8, C_FG);
    } else {
        iy = by + 6 + FONT_H + 8;
        window_fill_rect(TextEditor, bx + CELL_W - 2, iy - 2,
                         bw - 2 * CELL_W + 4, FONT_H + 4, C_BG);
        draw_string(TextEditor, pbuf, bx + CELL_W, iy, C_FG);
        if (phase)
            window_fill_rect(TextEditor,
                             bx + CELL_W + (u32)plen * CELL_W, iy,
                             FONT_W, 2, C_CUR);
            draw_string(TextEditor, hint, bx + CELL_W,
                        by + bh - FONT_H - 8, C_GUT);
    }
}

static const char *HELP_LINES[] = {
    "ZhirTE - keyboard reference (any key closes)",
    "",
    " Ctrl+O open          Ctrl+S save",
    " Ctrl+F find          F3 find next",
    " Ctrl+G go to line    Ctrl+Q quit",
    " Ctrl+Z undo          Ctrl+Y redo",
    " Ctrl+C copy / line   Ctrl+X cut",
    " Ctrl+V paste         Ctrl+A select all",
    "",
    " Arrows, Home, End, PgUp, PgDn, Ins",
    " Shift+Arrows select text",
    " Ctrl+Left/Right word jump",
    " Ctrl+Backspace delete word",
    " Tab indent, Esc cancel",
    "",
    " Syntax: ZhirScript / Python (auto)"
};

static void draw_help(u32 cw, u32 ch_px)
{
    u32 margin = 2 * CELL_W;
    u32 bw, bh, bx, by;
    size_t n = sizeof(HELP_LINES) / sizeof(HELP_LINES[0]);
    size_t i;
    size_t max_lines;

    if (cw < 8 * margin || ch_px < (u32)(n + 2) * FONT_H)
        return;

    bw = cw - 2 * margin;
    bh = (u32)(n + 2) * FONT_H;
    bx = BORDER + margin;
    by = TITLE_H + (ch_px - bh) / 2;

    window_fill_rect(TextEditor, bx + 3, by + 3, bw, bh, C_SHADOW);
    window_fill_rect(TextEditor, bx, by, bw, bh, C_PANEL);
    window_fill_rect(TextEditor, bx, by, bw, 1, C_PANEL_BR);
    window_fill_rect(TextEditor, bx, by + bh - 1, bw, 1, C_PANEL_BR);
    window_fill_rect(TextEditor, bx, by, 1, bh, C_PANEL_BR);
    window_fill_rect(TextEditor, bx + bw - 1, by, 1, bh, C_PANEL_BR);

    max_lines = (bw / CELL_W) - 1;
    for (i = 0; i < n; i++) {
        const char *s = HELP_LINES[i];
        if (te_strlen(s) > max_lines)
            s = "";   /* не рисуем то, что не влезает */
            draw_string(TextEditor, s, bx + CELL_W,
                        by + FONT_H + (u32)i * FONT_H, C_FG);
    }
}

static void redraw(void)
{
    u32 cw, ch, rows, cols, gw, r, c;
    bool syn_ok;

    if (!TextEditor)
        return;
    if (TextEditor->width < 2 * BORDER + CELL_W
        || TextEditor->height < TITLE_H + BORDER + FONT_H)
        return;

    cw = (u32)TextEditor->width - 2 * BORDER;
    ch = (u32)TextEditor->height - TITLE_H - BORDER;
    rows = text_rows();
    cols = text_cols();
    gw = gutter_width();

    /* подсветка включена, если колонки влезают в кэш цветов */
    syn_ok = cols > 0 && cols <= SYN_MAX_COLS;

    if (closed) {
        window_fill_rect(TextEditor, BORDER, TITLE_H, cw, ch, C_BG);
        draw_string(TextEditor, "ZhirTE closed.",
                    BORDER + CELL_W, TITLE_H + FONT_H, C_FG);
        dirty = false;
        return;
    }

    window_fill_rect(TextEditor, BORDER, TITLE_H, cw, ch, C_BG);

    /* подсветка текущей строки */
    if (!help_open && rows && cur_row >= scroll_top
        && cur_row - scroll_top < rows) {
        u32 y = TITLE_H + (u32)(cur_row - scroll_top) * FONT_H;
    window_fill_rect(TextEditor, BORDER, y, cw, FONT_H, C_CURLINE);
        }

        for (r = 0; r < rows; r++) {
            u32 row = scroll_top + r;
            u32 y = TITLE_H + r * FONT_H;
            char num[12];
            Line *ln;
            u32 x0;

            if (row >= tb.count)
                break;

            u32_to_str((u32)(row + 1), num);
            draw_string(TextEditor, num,
                        BORDER + (u32)(gw - 1 - te_strlen(num)) * CELL_W, y,
                        row == cur_row ? C_GUT_CUR : C_GUT);

            ln = &tb.lines[row];
            x0 = BORDER + (u32)gw * CELL_W;
            if (syn_ok)
                syn_fill_colors(ln, scroll_left, cols, syn_colors);
            for (c = 0; c < cols; c++) {
                size_t tc = scroll_left + c;
                u32 x = x0 + c * CELL_W;
                if (tc >= ln->len)
                    break;
                if (sel_active && in_sel(row, tc)) {
                    window_fill_rect(TextEditor, x, y, CELL_W, FONT_H, C_SEL);
                    draw_char(TextEditor, (u8)ln->data[tc], x, y, C_SEL_FG);
                } else {
                    draw_char(TextEditor, (u8)ln->data[tc], x, y,
                              syn_ok ? syn_colors[c] : C_FG);
                }
            }
        }

        /* курсор */
        if (!help_open && prompt_mode == PM_NONE && rows
            && cur_row >= scroll_top && cur_row - scroll_top < rows
            && cur_col >= scroll_left && cur_col - scroll_left < cols) {
            u32 cx = BORDER + (u32)(gw + (cur_col - scroll_left)) * CELL_W;
        u32 cy = TITLE_H + (u32)(cur_row - scroll_top) * FONT_H;
        if (phase) {
            if (cur_row < tb.count && cur_col < tb.lines[cur_row].len)
                window_fill_rect(TextEditor, cx, cy + FONT_H - 2,
                                 FONT_W, 2, C_CUR);
                else
                    window_fill_rect(TextEditor, cx, cy,
                                     FONT_W, FONT_H, C_CUR);
        }
            }

            draw_status(cw, rows);
            if (prompt_mode != PM_NONE)
                draw_prompt(cw, ch, rows);
    if (help_open)
        draw_help(cw, ch);

    dirty = false;
}

/* ===================== ввод ===================== */

static void select_all(void)
{
    if (!tb.count)
        return;
    sel_active = true;
    sel_row = 0;
    sel_col = 0;
    cur_row = tb.count - 1;
    cur_col = tb.lines[cur_row].len;
    close_group();
    follow_cursor();
    dirty = true;
}

static void handle_key(u32 code, bool shift, bool ctrl)
{
    char base = keyboard_map[code];
    char ch;

    if (help_open) {
        help_open = false;
        dirty = true;
        return;
    }
    if (code == SC_F1) {
        help_open = !help_open;
        dirty = true;
        return;
    }

    ch = shift ? keyboard_map_shift[code] : base;
    if (caps && base >= 'a' && base <= 'z')
        ch = shift ? base : (char)(base - 32);

    if (prompt_mode != PM_NONE) {
        prompt_key(code, ch);
        return;
    }
    if (search_mode) {
        search_key(code, ch);
        return;
    }

    if (ctrl) {
        switch (base) {
            case 'c': do_copy(false);       return;
            case 'x': do_copy(true);        return;
            case 'v': do_paste();           return;
            case 'z': do_undo();            return;
            case 'y': do_redo();            return;
            case 'f': start_search();       return;
            case 's': if (shift) start_saveas(); else save_current(); return;
            case 'o': start_open();         return;
            case 'g': prompt_open(PM_GOTO); return;
            case 'q': start_quit();         return;
            case 'a': select_all();         return;
            default: break;
        }
        if (code == SC_LEFT)  { move_word_left(shift);  return; }
        if (code == SC_RIGHT) { move_word_right(shift); return; }
        if (code == SC_HOME)  { cursor_to(0, 0, shift); return; }
        if (code == SC_END) {
            size_t lr = tb.count ? tb.count - 1 : 0;
            cursor_to(lr, tb.count ? tb.lines[lr].len : 0, shift);
            return;
        }
        if (code == SC_BSP) {
            if (sel_active) {
                delete_selection();
                return;
            }
            if (tb.count && (cur_col > 0 || cur_row > 0)) {
                move_word_left(true);
                delete_selection();
            }
            return;
        }
        return;
    }

    switch (code) {
        case SC_UP:    move_up(shift);    return;
        case SC_DOWN:  move_down(shift);  return;
        case SC_LEFT:  move_left(shift);  return;
        case SC_RIGHT: move_right(shift); return;
        case SC_HOME:  cursor_to(cur_row, 0, shift); return;
        case SC_END:   cursor_to(cur_row, tb.count ? tb.lines[cur_row].len : 0, shift); return;
        case SC_PGUP:  move_page(true, shift);  return;
        case SC_PGDN:  move_page(false, shift); return;
        case SC_INS:   ovr = !ovr; set_msg(ovr ? "Overwrite" : "Insert"); dirty = true; return;
        case SC_F3:    if (have_query) find_next(false); else start_search(); return;
        case SC_DEL:   do_delete();    return;
        case SC_BSP:   do_backspace(); return;
        case SC_ENTER: do_enter();     return;
        case SC_TAB:   do_tab();       return;
        case SC_ESC:   sel_active = false; dirty = true; return;
        default: break;
    }

    if (ch >= 32 && ch <= 126)
        do_type_char(ch);
}

static void poll_keyboard(void)
{
    bool *m = get_key_state_matrix();
    bool shift, ctrl;
    u32 code;

    if (!m)
        return;

    shift = m[SC_LSHIFT] || m[SC_RSHIFT];
    ctrl = m[SC_LCTRL];

    if (m[SC_CAPS] && !prev_keys[SC_CAPS]) {
        caps = !caps;
        dirty = true;
    }

    for (code = 0; code < 128; code++) {
        bool pressed = m[code];
        bool was = prev_keys[code];

        prev_keys[code] = pressed;

        if (pressed && !was) {
            held_code = code;
            held_next = ticks_now + REPEAT_DELAY;
            handle_key(code, shift, ctrl);
        } else if (!pressed && was && held_code == code) {
            held_code = 0;
        }
    }

    /* автоповтор: не для модификаторов и F1 */
    if (held_code && held_code != SC_LSHIFT && held_code != SC_RSHIFT
        && held_code != SC_LCTRL && held_code != SC_ALT
        && held_code != SC_CAPS && held_code != SC_F1) {
        if (m[held_code]
            && (u32)(ticks_now - held_next) < 0x80000000u) {
            held_next = ticks_now + REPEAT_RATE;
        handle_key(held_code, shift, ctrl);
            }
        }
}

/* ===================== жизненный цикл ===================== */

static void te_on_resize(Window *w, s32 old_w, s32 old_h)
{
    (void)w;
    (void)old_w;
    (void)old_h;
    follow_cursor();
    dirty = true;
}

static void te_reset_state(void)
{
    size_t i;

    buf_clear();
    tb.count = 0;
    buf_insert_empty_line(0);

    cur_row = 0;
    cur_col = 0;
    scroll_top = 0;
    scroll_left = 0;
    sel_active = false;
    ovr = false;
    caps = false;
    closed = false;
    modified = false;
    dirty = true;
    last_phase = false;

    close_group();
    free_undo_all();
    free_redo_all();

    str_copy(file_path, "/first.py");
    has_path = true;
    update_lang();

    query_len = 0;
    query[0] = 0;
    search_mode = false;
    have_query = false;
    help_open = false;
    prompt_mode = PM_NONE;
    status_msg[0] = 0;
    msg_until = 0;

    if (clip) {
        free(clip);
        clip = 0;
        clip_len = 0;
    }

    for (i = 0; i < 128; i++)
        prev_keys[i] = false;
    held_code = 0;
}

static void te_setup(void)
{
    TextEditor = window_create("ZhirTE");
    if (!TextEditor)
        return;

    TextEditor->window_on_changed = 1;
    TextEditor->task = TE_task;
    TextEditor->on_resize = te_on_resize;
    window_render(TextEditor);

    te_reset_state();

    g_disk = find_letter();
    if (g_disk) {
        g_file = open(g_disk, file_path);
        if (!g_file)
            g_file = create_file(g_disk, file_path);
        if (!g_file) {
            has_path = false;
            set_msg("Cannot create /first.py");
        } else {
            load_from(g_file);
            update_lang();
        }
    } else {
        has_path = false;
    }
    dirty = true;
}

static void ensure_buffer(void)
{
    if (tb.count == 0)
        buf_insert_empty_line(0);
}

static void TE(void)
{
    te_setup();
    if (!TextEditor)
        return;

    while (1) {
        if (closed) {
            if (dirty) {
                redraw();
                compositor_present();
            }
            yield();
            continue;
        }

        ticks_now = getticks();
        ensure_buffer();

        if (window_is_active(TextEditor)) {
            poll_keyboard();
        } else {
            held_code = 0;
        }

        phase = ((ticks_now / BLINK_HALF) & 1) != 0;
        if (dirty || (phase != last_phase && window_is_active(TextEditor))) {
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

    TE_task = create_task("ZhirTE", (void *)TE);
}
