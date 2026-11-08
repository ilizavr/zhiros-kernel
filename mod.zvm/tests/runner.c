#include "parse.h"
#include "lexer.h"
#include "interpret.h"
#include <stdarg.h>

typedef struct Script {
    char* data;
    unsigned len;
} Script;

static char script_buf[1 << 20];
static unsigned char heap[64 << 20];
static unsigned heap_used;
static int log_mode;

extern long syscall(long, ...);

#define SYS_READ  3
#define SYS_OPEN  5
#define SYS_CLOSE 6
#define SYS_LSEEK 19

static void* test_alloc(u32 size) {
    size = (size + 15u) & ~15u;
    if (heap_used + size > sizeof(heap)) return 0;
    void* p = heap + heap_used;
    heap_used += size;
    return p;
}

static void host_write(const char* s, unsigned n) {
    extern long write(int, const void*, unsigned long);
    write(1, s, n);
}

static void host_printf(char* fmt, ...) {
    char out[4096];
    int o = 0;
    va_list ap;
    va_start(ap, fmt);

    for (char* p = fmt; *p && o < (int)sizeof(out) - 16; p++) {
        if (*p != '%') {
            if (log_mode) {
                if (*p == '\n') log_mode = 0;
                continue;
            }
            out[o++] = *p;
            continue;
        }

        p++;
        switch (*p) {
            case 's': {
                char* s = va_arg(ap, char*);
                if (!s) s = "(null)";
                while (*s && o < (int)sizeof(out) - 1) {
                    if (log_mode) {
                        if (*s == '\n') log_mode = 0;
                        s++;
                        continue;
                    }
                    out[o++] = *s++;
                }
                break;
            }
            case 'c': {
                int c = va_arg(ap, int);
                if (!log_mode) out[o++] = (char)c;
                break;
            }
            case 'u': {
                unsigned v = va_arg(ap, unsigned);
                char t[16];
                int n = 0;
                if (!v) t[n++] = '0';
                while (v) { t[n++] = (char)('0' + v % 10); v /= 10; }
                if (!log_mode) while (n) out[o++] = t[--n];
                break;
            }
            case 'x': {
                unsigned v = va_arg(ap, unsigned);
                char t[16];
                int n = 0;
                if (!v) t[n++] = '0';
                while (v) {
                    int d = v & 15;
                    t[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
                    v >>= 4;
                }
                if (!log_mode) while (n) out[o++] = t[--n];
                break;
            }
            case 'd': {
                int v = va_arg(ap, int);
                char t[16];
                int n = 0;
                if (v < 0) { if (!log_mode) out[o++] = '-'; v = -v; }
                if (!v) t[n++] = '0';
                while (v) { t[n++] = (char)('0' + v % 10); v /= 10; }
                if (!log_mode) while (n) out[o++] = t[--n];
                break;
            }
            case 'f': {
                double d = va_arg(ap, double);
                int whole = (int)d;
                char t[16];
                int n = 0;
                if (!whole) t[n++] = '0';
                while (whole) { t[n++] = (char)('0' + whole % 10); whole /= 10; }
                if (!log_mode) {
                    while (n) out[o++] = t[--n];
                    out[o++] = '.';
                    double frac = d - (int)d;
                    if (frac < 0) frac = -frac;
                    for (int i = 0; i < 6; i++) {
                        frac *= 10.0;
                        int digit = (int)frac;
                        out[o++] = (char)('0' + digit);
                        frac -= digit;
                    }
                }
                break;
            }
            case '%':
                if (!log_mode) out[o++] = '%';
                break;
            default:
                if (!log_mode) { out[o++] = '%'; if (*p) out[o++] = *p; }
                break;
        }
    }

    out[o] = '\0';
    host_write(out, (unsigned)o);
    va_end(ap);
}

static void host_print_color(char* text, u32 color) {
    (void)text;
    (void)color;
    log_mode = 1;
}

static unsigned script_size(struct file* f) {
    return ((Script*)f->other_info)->len;
}

static unsigned script_read(struct file* f, void* buf, unsigned size, unsigned off) {
    Script* s = (Script*)f->other_info;
    if (off >= s->len) return 0;
    unsigned n = s->len - off;
    if (n > size) n = size;
    for (unsigned i = 0; i < n; i++) ((char*)buf)[i] = s->data[off + i];
    return n;
}

static int load_script(const char* path, unsigned* out_len) {
    long fd = syscall(SYS_OPEN, path, 0, 0);
    if (fd < 0) return 0;

    long size = syscall(SYS_LSEEK, fd, 0, 2);
    syscall(SYS_LSEEK, fd, 0, 0);
    if (size < 0 || size > (long)sizeof(script_buf)) {
        syscall(SYS_CLOSE, fd);
        return 0;
    }

    *out_len = (unsigned)syscall(SYS_READ, fd, script_buf, (unsigned long)size);
    syscall(SYS_CLOSE, fd);
    return 1;
}

int main(int argc, char** argv) {
    kalloc = test_alloc;
    printf = host_printf;
    print_color = host_print_color;
    open = 0;

    if (argc != 2) {
        host_write("usage: runner <script.zo>\n", 25);
        return 2;
    }

    unsigned len;
    if (!load_script(argv[1], &len)) {
        host_write("runner: cannot read script\n", 27);
        return 2;
    }

    Script script = { script_buf, len };
    struct file file;
    memset(&file, 0, sizeof(file));
    file.read = script_read;
    file.getsize = script_size;
    file.other_info = &script;

    parse(&file);
    interpret(nodes);
    return 0;
}
