/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */

#include "../lib/zhirtypes.h"
#include "../lib/string.h"
#include "../lib/zhirapi.h"
#include "../lib/ports.h"
#include "elfloader.h"
#include "time.h"
#include "../lib/hexdump.h"

struct fb_info*fb;

struct image{
    u16 width;
    u16 height;
    u32 bytes[0];
};

static void put_pixel(u32 x, u32 y, u32 color) {
    volatile u32 *pixel = (volatile u32*)(fb->fb_addr + y * fb->screen_pitch + x * fb->bpp);
    volatile u32 *pixel2 = (volatile u32*)(fb->shadow_fb_addr + y * fb->screen_pitch + x * fb->bpp);
    *pixel2 &= 0xFF000000;
    if(fb->bpp == 4) *pixel2 |= 0xFF000000;
    *pixel2 |= color;
    *pixel = *pixel2;
}

static void drawimage(struct image*img,int startx,int starty)
{
    for(int x = 0;x<img->width;x++)
        for(int y = 0;y<img->height;y++)
            for(int dx = 0;dx<fb->font_scale;dx++)
                for(int dy = 0;dy<fb->font_scale;dy++)
                    put_pixel((x*fb->font_scale+dx)/2+startx,(y*fb->font_scale+dy)/2+starty,img->bytes[y*img->width+x]);
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

static struct object * fetch(struct objectArray* args)
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
    print_color("\n                 Memory allocated: ",0xAAAAFF);
    printf("%Z/%Z",getused()>>10,(getused()+getfree())>>10);

    free(fb);

    for(int i = logo_img->height/9-5;i>=0;i--)printf("\n");

    fb = getfb();
    drawimage(logo_img, 0, fb->cury - logo_img->height*fb->font_scale/2);

    logo->close(logo);
    free(fb);
    free(logo_img);
}



static void img(char disk, char* name)
{
    struct file *logo = open(disk,name);
    int size = logo->getsize(logo);
    struct image *logo_img = alloc(size);

    logo->read(logo, logo_img, size, 0);
    logo->close(logo);

    fb = getfb();
    drawimage(logo_img, fb->screen_width-logo_img->width, 0);
    free(logo_img);
    free(fb);
}
static void clitools()
{
    while(true) {
        if((getticks()%500)>100)printf("\xDB\b");
        else printf(" \b");
        yield();
    }
}

static struct object *reboot(struct objectArray *args)
{
    CLI();
    u8 good = 0x02;
    while(good&0x02)good=inb(0x64);

    outb(0x64,0xfe);

    return 0;
}

static struct object *date(struct objectArray* args)
{
    struct rtc_time now = get_time();

    printf("%u.%u.%u    %u:%u:%u\n",now.day,now.month,now.year+2000,now.hour,now.minute,now.second);

    return 0;
}

static void dumpmem(void* addr, i_ptr size)
{
    if(size>65536)
    {
        LOGW("memory block is big. continue?");
        char chr = getchar();
        if(chr!='y') return;
    }
    hexdump(addr,size);
}
static void dumpdisk(u32 idx, u32 lba, u32 blocks)
{
    if(blocks>32)
    {
        LOGW("many blocks. continue?");
        char chr = getchar();
        if(chr!='y') return;
    }
    struct disk* d = getdisk(idx);
    if(!d){
        LOGE("disk not found");
        return;
    }

    void *buf = alloc(blocks*512);
    d->lba_read(d,lba,buf,blocks);

    hexdump(buf,blocks*512);
    free(buf);
}

static struct object * cls(struct objectArray* args)
{
    clearframe();
    return 0;
}

static struct object * lsmod(struct objectArray* args)
{
    struct module_info * mdls = get_module_array();
    printf("ID   LOCATION        NAME\n");
    for(int i = 0; i< 256;i++) if(mdls[i].start) printf("%u    %x-%x   %s\n",i, mdls[i].start,mdls[i].start+mdls[i].size,mdls[i].path);

    return 0;
}
static void printfile(char diskletter, char* name)
{
    struct file* f = open(diskletter,name);
    if(!f)
    {
        LOGE("file %s not found", name);
        return;
    }

    int size = f->getsize(f);
    if(size>65536)
    {
        LOGW("file is big. continue?");
        char chr = getchar();
        if(chr!='y'){
            f->close(f);
            return;
        }
    }
    char *buffer = alloc(size+1);
    f->read(f,buffer,size,0);
    buffer[size] = 0;
    printf("%s",buffer);
    f->close(f);
    free(buffer);
}

static struct object* ps(struct objectArray* args)
{
    struct task* tasks = get_tasks();
    printf("TID  NAME\n");
    for(int i = 0;i<MAX_TASKS;i++)
    {
        if(tasks[i].tid!=0) printf("%u    %s\n",tasks[i].tid,tasks[i].name);
    }
    return NULL;
}

static void print_tree(struct disk*d, u32 depth)
{
    for(int i = 0;i<depth;i++)printf("  ");
    if(depth>0)printf("\b\b|-");

    printf("%s",d->name);
    for(int i = (strlen(d->name)+depth*2);i<18;i++) printf(" ");
    printf("%c       %Z\n",d->mountletter,((u64)d->size)>>1);

    for(int i = 0;i<MAX_PART_COUNT;i++)
    {
        struct disk* dsk = d->partitions[i];
        if(!dsk) continue;
        print_tree(dsk,depth+1);
    }
}

static struct object* lsblk(struct objectArray* args)
{
    printf("NAME              MOUNT   SIZE\n");

    for(int i = 0;i<MAX_DISK_COUNT;i++)
    {
        struct disk* dsk = getdisk(i);
        if(!dsk) continue;
        print_tree(dsk,0);
    }

    return 0;
}

INIT void init(void* (*_resolve_function)(char* name))
{
    api_init(_resolve_function);

    register_function("fetch",fetch,"print short system information");
    register_function("reboot",reboot,"reboot system now");
    register_function("date",date,"current date and time");
    register_function("lsmod",lsmod,"print all loaded modules");
    register_function("cls",cls,"clear screen");
    register_function("ps",ps,"proccess list");
    register_function("lsblk",lsblk,"print all disk");
    register_function("_pf",printfile,"print file. use _pf;c:diskletter;name");
    register_function("_dmp",dumpmem,"hexdump ram. _dmp;i:0xaddr;i:size");
    register_function("_dmpdsk",dumpdisk,"hexdump disk. _dmpdsk;i:diskidx;i:lba;i:blocks");
    register_function("_elfload",elf_load,"_elfload(char diskletter,char* name) -> bool success");
    register_function("_img",img,"_img;c:diskletter;name. print image in fbcon");

    // struct disk* testdisk = alloc(sizeof(struct disk));
    // testdisk->size = 1024*1024*1024;
    // testdisk->name = "test1";
    // testdisk->mountletter=0;
    // memset(testdisk->partitions,0,sizeof(void*)*MAX_PART_COUNT);
    //
    //
    // struct disk* testdisk2 = alloc(sizeof(struct disk));
    // testdisk2->size = 1024*1024*1024;
    // testdisk2->name = "test2";
    // testdisk2->mountletter=0;
    // memset(testdisk2->partitions,0,sizeof(void*)*MAX_PART_COUNT);
    // testdisk->partitions[0] = testdisk2;
    //
    // struct disk* testdisk3 = alloc(sizeof(struct disk));
    // testdisk3->size = 1024*1024*1024;
    // testdisk3->name = "test3";
    // testdisk3->mountletter=0;
    // memset(testdisk3->partitions,0,sizeof(void*)*MAX_PART_COUNT);
    // testdisk2->partitions[0] = testdisk3;
    //
    // diskadd(testdisk);



    create_task("clitools thread",clitools);
}
