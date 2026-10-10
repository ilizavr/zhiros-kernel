/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */

#include "../lib/zhirtypes.h"
#include "../lib/string.h"

static struct fb_info* (*getfb)();
static i_ptr (*getfree)();
static void (*register_function)(char *function_name, void* call, char *description);
static struct file* (*open)(char diskletter, char* path);
static void (*free)(void*);
static void* (*alloc)(u32 size);
static void (*printf)(...);
static void (*print_color)(char*,u32);
static u32 (*getticks)();
static bool (*hook_interrupt)(u32 n, void* function);
static void* (*resolve_function)(char* name);
struct task* (*create_task)(char* name, void* function);
void (*yield)();

struct fb_info*fb;

struct image{
    u16 width;
    u16 height;
    u32 bytes[0];
};

static void put_pixel(u32 x, u32 y, u32 color) {
    volatile u32 *pixel = (volatile u32*)(fb->fb_addr + y * fb->screen_pitch + x * fb->bpp);
    *pixel &= 0xFF000000;
    if(fb->bpp == 4) *pixel |= 0xFF000000;
    *pixel |= color;
}

static void drawimage(struct image*img,int startx,int starty)
{
    for(int x = 0;x<img->width;x++)
        for(int y = 0;y<img->height;y++)
            put_pixel(x+startx,y+starty,img->bytes[y*img->width+x]);
}

static void cpuid(u32 code, u32*a, u32*b, u32*c, u32*d)
{
    asm volatile("cpuid"
    : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
    : "a" (code));
}

static void get_cpu_model(char * buffer)
{
    u32 *ptr = (u32*)buffer;
    u32 a,b,c,d;
    for(u32 i = 0;i<3;i++){
        cpuid(0x80000002+i, &a,&b,&c,&d);
        ptr[0]=a;
        ptr[1]=b;
        ptr[2]=c;
        ptr[3]=d;
        ptr+=4;
    }
    buffer[48] = 0;
}

static void fetch()
{
    char buffer[256];
    get_cpu_model(buffer);

    struct file *logo = open('A',"./logo.zi");
    int size = logo->getsize(logo);
    struct image *logo_img = alloc(size);
    logo->read(logo, logo_img, size, 0);

    fb = getfb();

    u32 ticks = getticks()/1000;

    print_color("\n                 OS: ",0xAAAAFF);
    printf("ZHIROS");
    print_color("\n                 CPU: ",0xAAAAFF);
    printf("%s",buffer);
    print_color("\n                 Display: ",0xAAAAFF);
    printf("%ux%u",fb->screen_width,fb->screen_height);
    print_color("\n                 Uptime: ",0xAAAAFF);
    if(ticks>3600) printf("%uh ", ticks/3600);
    if(ticks>60) printf("%um ", (ticks%3600)/60);
    printf("%us", ticks%60);
    print_color("\n                 Free memory: ",0xAAAAFF);
    printf("%uM",getfree()>>20);

    free(fb);

    for(int i = logo_img->height/9-5;i>=0;i--)printf("\n");

    fb = getfb();
    drawimage(logo_img, fb->curx, fb->cury - logo_img->height);

    logo->close(logo);
    free(fb);
    free(logo_img);
}

#include "elfloader.h"

static struct image *logo_img = 0;

static void img(char disk, char* name)
{
    if(logo_img) free(logo_img);

    struct file *logo = open(disk,name);
    int size = logo->getsize(logo);
    logo_img = alloc(size);

    logo->read(logo, logo_img, size, 0);
    logo->close(logo);
}
static void imgview()
{
    while(true) {
        if(logo_img){
            fb = getfb();
            drawimage(logo_img, fb->screen_width-logo_img->width, 0);
        }
        yield();
    }
}

INIT void init(void* (*_resolve_function)(char* name))
{
    resolve_function = _resolve_function;

    getfb = _resolve_function("_getfb");
    open = _resolve_function("_open");
    free = _resolve_function("_free");
    alloc = _resolve_function("_alloc");
    printf = _resolve_function("_printf");
    print_color = _resolve_function("_print_color");
    getfree = _resolve_function("_getfree");
    getticks = _resolve_function("_getticks");
    register_function = _resolve_function("_register_function");
    hook_interrupt = _resolve_function("_hook_interrupt");
    create_task = _resolve_function("_create_task");
    yield = _resolve_function("_yield");

    register_function("fetch",fetch,"print short system information");
    register_function("_elfload",elf_load,"_elfload(char diskletter,char* name) -> bool success");
    register_function("_img",img,"_img c:diskletter name. print image in fbcon");

    create_task("img renderer",imgview);
}
