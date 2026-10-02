#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#ifndef ZHIRTYPES
#define ZHIRTYPES

/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */


typedef unsigned int u32;
typedef signed int s32;
typedef unsigned int size_t;
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned long long u64;
typedef signed long long s64;

typedef unsigned int i_ptr;

#define false 0
#define true 1

#define INLINE static inline __attribute__((always_inline, optimize("O3")))
#define PAK __attribute__((packed))
#define API
#define INIT __attribute__((section(".text.init")))

#define CALL(addr,...) ((void(*)(...))addr)(__VA_ARGS__)
#define HLT() asm volatile("hlt")
#define STI() asm volatile("sti")
#define CLI() asm volatile("cli")

#define OBJECT_STRING 0

#define LOGI(...) do { \
print_color("[I] ", 0x00FF00); \
printf(__VA_ARGS__); \
printf("\n");\
} while(0)
#define LOGE(...) do { \
print_color("[E] ", 0xFF0000); \
printf(__VA_ARGS__); \
printf("\n");\
} while(0)
#define LOGW(...) do { \
print_color("[W] ", 0xAA00FF); \
printf(__VA_ARGS__); \
printf("\n");\
} while(0)


PAK struct object
{
    u32 type;
    u32 len;
    void* data;
};

PAK struct objectArray
{
    u32 count;
    struct object objs[0];
};

struct function_info
{
    void* call;
    char * name;
    char * description;
    struct function_info* next;
};
struct module_info
{
    char *path;
    void *start;
    u32 size;
};

struct fb_info
{
    i_ptr fb_addr;
    u32 screen_width;
    u32 screen_height;
    u32 bpp;
    u32 screen_pitch;

    u32 curx;
    u32 cury;
};

struct disk//эта же структура используется для разделов диска
{
    char *name;
    u32 size;//in sectors

    bool (*lba_read)(struct disk* dsk, u32 lba, char* buffer, u32 blocks);
    bool (*lba_write)(struct disk* dsk, u32 lba, char* buffer, u32 blocks);

    void* other_info;
    void* fs_info;
};

struct file // это также директория
{
    struct disk* dsk;
    char *path;
    bool is_dir;

    u32 (*read)(struct file* file, void* buffer, u32 size, u32 offset);// read от директории читает имяфайла\nимя2файла и тд. крч read от директории выглядит как ls
    u32 (*write)(struct file* file, void* buffer, u32 size, u32 offset);
    u32 (*getsize)(struct file* file); //размер файла

    bool (*close)(struct file* file);

    void* other_info;
};

static const char keyboard_map[128] =
{
    0,   0, '1', '2', '3', '4', '5', '6', '7', '8', /* 9 */
    '9', '0', '-', '=', '\b', /* Backspace */
    ' ',                     /* Tab */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    '\n',                     /* Enter (Скан-код 0x1C) */
    0,                      /* Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,                      /* Left Shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0,                      /* Right Shift */
    0,
    0,                      /* Alt */
    ' ',                      /* Space */
    0,                      /* Caps lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* F1-F10 keys */
    0,                      /* Num lock */
    0,                      /* Scroll lock */
    0,                      /* Home key */
    24,                      /* Up Arrow */
    0,                      /* Page Up */
    '-',
    27, /* Left Arrow */
    0,
    26,                      /* Right Arrow */
    '+',
    0,                      /* End key */
    25,                      /* Down Arrow */
    0,                      /* Page Down */
    0,                      /* Insert Key */
    0,                      /* Delete Key */
    0, 0, 0,
    0,                      /* F11 Key */
    0,                      /* F12 Key */
    0,
};
static const char keyboard_map_shift[128] =
{
    0,   0, '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+', '\b',
    ' ',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n',
    0,                      /* Control */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~',
    0,                      /* Left Shift */
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0,                      /* Right Shift */
    0,
    0,                      /* Alt */
    ' ',                      /* Space */
    0,                      /* Caps lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* F1-F10 keys */
    0,                      /* Num lock */
    0,                      /* Scroll lock */
    0,                      /* Home key */
    24,                      /* Up Arrow */
    0,                      /* Page Up */
    0,
    27, /* Left Arrow */
    0,
    26,                      /* Right Arrow */
    0,
    0,                      /* End key */
    25,                      /* Down Arrow */
    0,                      /* Page Down */
    0,                      /* Insert Key */
    0,                      /* Delete Key */
    0, 0, 0,
    0,                      /* F11 Key */
    0,                      /* F12 Key */
    0,
};

static const u8 ascii_to_keycode[128] =
{
    /* 0x00 - 0x07 */ 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x08 - 0x0F */ 14, 0, 28, 0, 0, 0, 0, 0,        /* 0x08='\b', 0x0A='\n' */
    /* 0x10 - 0x17 */ 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x18 - 0x1F */ 72, 80, 77, 75, 0, 0, 0, 0,      /* 24=Up, 25=Down, 26=Right, 27=Left */
    /* 0x20 - 0x27 */ 57, 0, 0, 0, 0, 0, 0, 40,        /* 0x20=' ', 0x27='\'' */
    /* 0x28 - 0x2F */ 0, 0, 0, 78, 51, 74, 52, 53,     /* 0x2B='+', 0x2C=',', 0x2D='-', 0x2E='.', 0x2F='/' */
    /* 0x30 - 0x37 */ 11, 2, 3, 4, 5, 6, 7, 8,         /* '0'-'7' */
    /* 0x38 - 0x3F */ 9, 10, 0, 39, 0, 13, 0, 0,       /* '8'-'9', 0x3B=';', 0x3D='=' */
    /* 0x40 - 0x47 */ 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x48 - 0x4F */ 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x50 - 0x57 */ 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x58 - 0x5F */ 0, 0, 0, 26, 43, 27, 0, 0,       /* 0x5B='[', 0x5C='\\', 0x5D=']' */
    /* 0x60 - 0x67 */ 41, 30, 48, 46, 32, 18, 33, 34,  /* 0x60='`', 'a', 'b', 'c', 'd', 'e', 'f', 'g' */
    /* 0x68 - 0x6F */ 35, 23, 36, 37, 38, 50, 49, 24,  /* 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o' */
    /* 0x70 - 0x77 */ 25, 16, 19, 31, 20, 22, 47, 17,  /* 'p', 'q', 'r', 's', 't', 'u', 'v', 'w' */
    /* 0x78 - 0x7F */ 45, 21, 44, 0, 0, 0, 0, 0         /* 'x', 'y', 'z' */
};


static inline u32 save_irq()
{
    u32 eflags;
    asm volatile("pushfl; pop %0; cli" : "=r"(eflags));
    return eflags;
}
static inline void restore_irq(u32 eflags)
{
    asm volatile("push %0; popfl" : : "r"(eflags));
}

static void crashde()
{
    u32 test = 1/0;
}
#endif
