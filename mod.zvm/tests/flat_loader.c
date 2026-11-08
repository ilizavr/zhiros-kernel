#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/mman.h>
#include "../../lib/zhirtypes.h"

static char* script_data;
static unsigned script_len;
static int log_mode;

static void* my_alloc(u32 size) {
    return malloc(size);
}

static void my_printf(char* fmt, ...) {
    if (log_mode) {
        if (strchr(fmt, '\n')) log_mode = 0;
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

static void my_color(char* text, u32 color) {
    (void)text;
    (void)color;
    log_mode = 1;
}

static unsigned script_size(struct file* f) {
    (void)f;
    return script_len;
}

static unsigned script_read(struct file* f, void* buf, unsigned size, unsigned off) {
    (void)f;
    if (off >= script_len) return 0;
    unsigned n = script_len - off;
    if (n > size) n = size;
    memcpy(buf, script_data + off, n);
    return n;
}

static struct file* my_open(char disk, char* path) {
    (void)disk;
    (void)path;
    struct file* f = malloc(sizeof(*f));
    memset(f, 0, sizeof(*f));
    f->read = script_read;
    f->getsize = script_size;
    return f;
}

static void* resolve(char* name) {
    if (!strcmp(name, "_alloc")) return (void*)my_alloc;
    if (!strcmp(name, "_printf")) return (void*)my_printf;
    if (!strcmp(name, "_print_color")) return (void*)my_color;
    if (!strcmp(name, "_open")) return (void*)my_open;
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: flat_loader <script.zo> <module.mod>\n");
        return 2;
    }

    FILE* sf = fopen(argv[1], "rb");
    if (!sf) {
        perror("open script");
        return 2;
    }
    fseek(sf, 0, SEEK_END);
    script_len = (unsigned)ftell(sf);
    fseek(sf, 0, SEEK_SET);
    script_data = malloc(script_len);
    fread(script_data, 1, script_len, sf);
    fclose(sf);

    FILE* mf = fopen(argv[2], "rb");
    if (!mf) {
        perror("open module");
        return 2;
    }
    fseek(mf, 0, SEEK_END);
    long module_size = ftell(mf);
    fseek(mf, 0, SEEK_SET);
    unsigned char* module_data = malloc((size_t)module_size);
    fread(module_data, 1, (size_t)module_size, mf);
    fclose(mf);

    void* low = mmap((void*)0x10000, 0x20000,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (low == MAP_FAILED) {
        perror("mmap low memory");
        return 2;
    }
    memset(low, 0xA5, 0x20000);

    unsigned char* base = mmap(NULL, (size_t)module_size,
                               PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) {
        perror("mmap module");
        return 2;
    }
    memcpy(base, module_data, (size_t)module_size);
    free(module_data);

    void (*module_init)(void* (*)(char*)) = (void (*)(void* (*)(char*)))base;
    module_init(resolve);
    return 0;
}
