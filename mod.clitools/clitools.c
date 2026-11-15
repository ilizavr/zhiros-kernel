/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */

#include "../lib/zhirtypes.h"
#include "../lib/string.h"
#include "../lib/zhirapi.h"
#include "../lib/ports.h"
#include "../lib/hexdump.h"
#include "elfloader.h"
#include "time.h"

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

static void drawimage(struct image*img,int startx,int starty, int font_scale)
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

static struct object * fetch(struct objectArray* args)
{
    char buffer[256];
    get_cpu_model(buffer);

    struct file *logo = open('A',"./logo.zi");
    if(!logo) return 0;
    int size = logo->getsize(logo);
    struct image *logo_img = alloc(size);
    logo->read(logo, logo_img, size, 0);

    fb = getfb();

    u32 ticks = getticks()/1000;

    print_color("                 OS: ",0xFFA500);
    printf("ZHIROS");
    print_color("\n                 Kernel: ",0xFFA500);
    printf("ZHIR-KERNEL(%x)", get_kernel_checksum());
    print_color("\n                 CPU: ",0xFFA500);
    printf("%s",buffer);
    print_color("\n                 Display: ",0xFFA500);
    printf("%ux%u",fb->screen_width,fb->screen_height);
    print_color("\n                 Uptime: ",0xFFA500);
    if(ticks>3600) printf("%uh ", ticks/3600);
    if(ticks>60) printf("%um ", (ticks%3600)/60);
    printf("%us", ticks%60);
    print_color("\n                 Memory allocated: ",0xFFA500);
    printf("%Z/%Z\n",getused()>>10,(getused()+getfree())>>10);

    int i = 6;
    for( ; i <= logo_img->height/fb->font_height ; i++) printf("\n");

    free(fb);
    fb = getfb();
    drawimage(logo_img, 0, fb->cury - fb->font_height*i, (fb->font_height+4)/9);

    logo->close(logo);
    free(fb);
    free(logo_img);
}



static void img(char disk, char* name)
{
    struct file *logo = open(disk,name);
    if(!logo) return;
    int size = logo->getsize(logo);
    struct image *logo_img = alloc(size);

    logo->read(logo, logo_img, size, 0);
    logo->close(logo);

    fb = getfb();
    drawimage(logo_img, fb->screen_width-logo_img->width, 0,1);
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
static void dumpdisk(u32 diskidx, u32 lba, u32 blocks)
{
    struct disk *d = (struct disk *)diskidx;
    if(diskidx<MAX_DISK_COUNT)d = getdisk(diskidx);

    if(!d)
    {
        LOGE("disk not found!");
        return;
    }

    if(blocks>32)
    {
        LOGW("many blocks. continue?");
        char chr = getchar();
        if(chr!='y') return;
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
    printf("ID   LOCATION        CHECKSUM  NAME\n");
    for(int i = 0; i< 256;i++){
        if(!mdls[i].start) continue;
        struct module_info module = mdls[i];
        struct module_header* mh = (struct module_header*)module.start;

        printf("%u    %x-%x   %C%x     %C%s%C\n",i, module.start,module.start+module.size,0x00FF00,crc16((u8*)module.start,mh->text_end),0xFFA500,module.path,0xAAAAAA);
    }

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
            if(f->close) f->close(f);
            return;
        }
    }
    char *buffer = alloc(size+1);
    f->read(f,buffer,size,0);
    buffer[size] = 0;
    printf("%s",buffer);
    if(f->close)f->close(f);
    free(buffer);
}


static struct object* ps(struct objectArray* args)
{
    struct task* tasks = get_tasks();
    printf("TID  TASK               MODULE\n");
    for(int i = 0;i<MAX_TASKS;i++)
    {
        if(tasks[i].tid==0) continue;
        printf("%u    %s",tasks[i].tid,tasks[i].name);
        for(int j = strlen(tasks[i].name);j<19;j++)printf(" ");

        if(tasks[i].module_idx == -1){
            printf("kernel\n");
            continue;
        }
        struct module_info* mdls = get_module_array();
        printf("%s\n",mdls[tasks[i].module_idx].path);
    }
    return NULL;
}

static void print_tree(struct disk*d, u32 depth)
{
    for(int i = 0;i<depth;i++)printf("  ");
    if(depth>0)printf("\b\b|-");

    printf("%s",d->name);
    for(int i = (strlen(d->name)+depth*2);i<18;i++) printf(" ");
    printf("%c       %Z    0x%x\n",d->mountletter,d->size>>1, d);

    for(int i = 0;i<MAX_PART_COUNT;i++)
    {
        struct disk* dsk = d->partitions[i];
        if(!dsk) continue;
        print_tree(dsk,depth+1);
    }
}

static struct object* lsblk(struct objectArray* args)
{
    printf("NAME              MOUNT   SIZE   STRUCT_DISK_ADDR\n");

    for(int i = 0;i<MAX_DISK_COUNT;i++)
    {
        struct disk* dsk = getdisk(i);
        if(!dsk) continue;
        print_tree(dsk,0);
    }

    return 0;
}

static void ls(char diskletter, char* name) {
    struct file* f = open(diskletter,name);
    struct file entrites[8];
    if(!f)
    {
        LOGE("dir %s not found", name);
        return;
    }

    u32 bytes = f->read(f, entrites, sizeof(entrites), 0);
    u32 count = bytes / sizeof(entrites[0]);

    u32 idx = 0;

    for (u32 i = 0; i < count; i++){
        if(entrites[i].is_dir) printf("%C", 0x0088FF);

        char *ext = entrites[i].path+strlen(entrites[i].path)-4;
        if(!strcmp(ext,".mod")) printf("%C", 0xFFA500);
        if(!strcmp(ext,".elf")) printf("%C", 0x55FF55);
        printf("%s%C", entrites[i].path,0xAAAAAA);
        for(int i = strlen(entrites[i].path);(i%20)>0;i++)printf(" ");
        idx += strlen(entrites[i].path)/20;
    }
}

static void writefile(char diskletter, char* name) {
    struct file * ff = open(diskletter,name);
    if(!ff && create_file) ff = create_file(diskletter,name);
    if(!ff || !create_file || !ff->write) { LOGE("fs not support this"); return; }

    printf("to stop writing enter Q\n");

    char* buffer = alloc(8192);
    char* curptr = buffer;
    for(;;)
    {
        int recv = gets(curptr,256);
        if(curptr[0] == 'Q' && curptr[1] == 0) {curptr[0] = 0; break;}

        curptr+= recv;
        curptr[0] = '\n';
        curptr++;
    }

    ff->write(ff, buffer, strlen(buffer),0);


    if(ff->close) ff->close(ff);
}
#include <stddef.h>
#include <stdint.h>


struct ustar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} __attribute__((packed));

static u32 parse_octal(const char *str, size_t size) {
    u32 val = 0;
    for (size_t i = 0; i < size; i++) {
        char c = str[i];
        if (c == '\0' || c == ' ') {
            break;
        }
        if (c >= '0' && c <= '7') {
            val = (val << 3) | (c - '0');
        }
    }
    return val;
}

static void printramdisk()
{
    struct disk* rd = getdisk(0);
    if (!rd) return;

    struct ustar_header hdr;
    u32 lba = 0;
    u32 idx = 0;

    while (1) {
        rd->lba_read(rd, lba, (char*)&hdr, 1);

        if (hdr.name[0] == '\0') {
            break;
        }

        if((idx++ % 3) == 0 && idx>1) printf("\n");

        u32 size = parse_octal(hdr.size, 11);

        if(hdr.typeflag == '5') printf("%C", 0x0088FF);

        char *ext = hdr.name+strlen(hdr.name)-4;
        if(!strcmp(ext,".mod")) printf("%C", 0xFFA500);
        if(!strcmp(ext,".elf")) printf("%C", 0x55FF55);
        printf("%s%C", hdr.name,0xAAAAAA);
        for(int i = strlen(hdr.name);(i%20)>0;i++)printf(" ");
        idx += strlen(hdr.name)/20;

        u32 sectors = (size + 511) / 512;

        lba += 1 + sectors;
    }
    printf("\n");
}

static u32 cat_icon1[16*16] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0xADF37D17, 0x7FEE7A16, 0x02FF7F00, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x02FF7F00, 0x80F37D17, 0xACEB7916, 0x00000000, 0x00000000,
    0x00000000, 0x1AF57F13, 0xFEC7A88E, 0xFFBFA48C, 0xC5D08547, 0x09C6551C, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x09FF711C, 0xCBE59B5A, 0xFFC9AD96, 0xFED0B39D, 0x19CC6614, 0x00000000,
    0x00000000, 0x49ED7A18, 0xFFBA997E, 0xFFAD8B73, 0xFFB8987E, 0xF0CF864E, 0xEBF47D16, 0xFDF58017, 0xFDF58017, 0xEAF57F16, 0xF2E4995B, 0xFFBDA085, 0xFFAF8D75, 0xFFBEA28C, 0x47BA5D11, 0x00000000,
    0x00000000, 0x47E57615, 0xFFAC8D76, 0xFFA27C5E, 0xFFE48029, 0xFFF6841B, 0xFFF78A21, 0xFFF98F26, 0xFFF98F26, 0xFFF78A21, 0xFFF6841B, 0xFFE48029, 0xFFA27C5E, 0xFFB18F79, 0x47A14B0E, 0x00000000,
    0x00000000, 0x2DD76B16, 0xFFB1896B, 0xFFEE811F, 0xFFF78D24, 0xFFFA9B30, 0xFFFA9F35, 0xFFFCA93E, 0xFFFBA83E, 0xFFFA9F35, 0xFFFA9B30, 0xFFF78C24, 0xFFEE801F, 0xFFB18B6E, 0x2B88410B, 0x00000000,
    0x00000000, 0x05CC6600, 0xFFED8225, 0xFFF99229, 0xFFFBA93E, 0xFFFEB147, 0xFFFDAE44, 0xFFFFB349, 0xFFFFB349, 0xFFFEAE44, 0xFFFEB046, 0xFFFCA83F, 0xFFF89128, 0xFFEE8426, 0x047F3F00, 0x00000000,
    0x00000000, 0x59F67E16, 0xFFF78920, 0xFFFBA43B, 0xFFFDB44F, 0xFFFFC470, 0xFFFEB856, 0xFFFFB54C, 0xFFFFB54C, 0xFFFFB956, 0xFFFEC470, 0xFFFDB34E, 0xFFFBA43B, 0xFFF78820, 0x5AF67C16, 0x00000000,
    0x00000000, 0xBEF47F16, 0xFFF8932A, 0xFFFA9C32, 0xFFBB661D, 0xFF687033, 0xFF8D5F1F, 0xFFFFB74D, 0xFFFFB74D, 0xFF8E6224, 0xFF656C2D, 0xFFBB661E, 0xFFFA9B32, 0xFFF89228, 0xBDF47E16, 0x00000000,
    0x00000000, 0xD7F57E17, 0xFFF9952E, 0xFFFCA93F, 0xFFB87932, 0xFF7EA643, 0xFF606423, 0xFFFFB74D, 0xFFFFB74D, 0xFF525D20, 0xFF7EA643, 0xFFB97A33, 0xFFFCA83F, 0xFFFA962F, 0xD7F57E17, 0x00000000,
    0x00000000, 0xC2F47F17, 0xFFF88E26, 0xFFFBA847, 0xFFFCB04F, 0xFFE19C40, 0xFFEA9135, 0xFFE4A557, 0xFFE3A457, 0xFFDE8631, 0xFFE19C3F, 0xFFFCB04F, 0xFFFBA746, 0xFFF88D26, 0xC0F57F17, 0x00000000,
    0x00000000, 0x61D7761F, 0xFDE7822B, 0xFFE6A061, 0xFFEAA86A, 0xFFF4BE76, 0xFFD29051, 0xFF8B5A3E, 0xFF8A5A3E, 0xFFD6914E, 0xFFF4BD76, 0xFFEAA86A, 0xFFE6A061, 0xFDE7822B, 0x5FD97320, 0x00000000,
    0x00000000, 0x1E995D3B, 0xA3DE7622, 0xFDE59044, 0xFFE2AE7C, 0xFFE1DCD2, 0xFFE2DFD7, 0xFFB27E59, 0xFFB37E5A, 0xFFE2DED5, 0xFFE1DCD2, 0xFFE2AE7C, 0xFDE48F44, 0xA2DD7622, 0x1E995D3B, 0x00000000,
    0x00000000, 0x00000000, 0x11965A3C, 0x62C56A2C, 0xD8E38E43, 0xFDE1D0C2, 0xFFE1DBD4, 0xFFE0DAD3, 0xFFE0DAD3, 0xFFE0DBD4, 0xFDE0D0C1, 0xD7E38E42, 0x61C56B2C, 0x11965A3C, 0x00000000, 0x00000000,
    0x00000000, 0x098D5538, 0x1499593F, 0x00000000, 0x00000000, 0x00000000, 0x70EAAF7F, 0xD9E0C7B2, 0xD9E0C7B2, 0x6EEAB281, 0x00000000, 0x00000000, 0x00000000, 0x1499593F, 0x098D5538, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000
};
static u32 cat_icon2[16*16]  = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0xA1BA988E, 0x9A8E8B8C, 0x45607D8C, 0x027F7F7F, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x027F7F7F, 0x465E7B8A, 0x9B8D8A8B, 0xA2B9978D, 0x00000000,
    0x00000000, 0xD8D29E8E, 0xFFFDAA91, 0xFFAA938E, 0xDF607C8B, 0x585F7C8B, 0x466D8695, 0x7F6E8892, 0x7F6E8892, 0x456B8593, 0x59617E8C, 0xDF607C8B, 0xFFA08F8D, 0xFFFDAA91, 0xD7D19D8F, 0x00000000,
    0x00000000, 0xCCCE9D8E, 0xFFFFAB91, 0xFFFFAB91, 0xFF828B93, 0xFF748E9A, 0xFF78909C, 0xFF78909C, 0xFF78909C, 0xFF78909C, 0xFF748E9A, 0xFF848D93, 0xFFFFAB91, 0xFFFFAB91, 0xCCCE9D8E, 0x00000000,
    0x00000000, 0xB6C89B90, 0xFFFDAA91, 0xFF958E8F, 0xFF708996, 0xFF778F9B, 0xFF78909C, 0xFF78909C, 0xFF78909C, 0xFF78909C, 0xFF778F9B, 0xFF6F8996, 0xFF968E8F, 0xFFFEAB91, 0xB6C89B90, 0x00000000,
    0x00000000, 0x83BC998E, 0xFFAB928E, 0xFF607D8B, 0xFF607D8B, 0xFF64808D, 0xFF758E9A, 0xFF78909C, 0xFF78909C, 0xFF758E9A, 0xFF65818E, 0xFF607D8B, 0xFF607D8B, 0xFFAC938D, 0x81BD988E, 0x00000000,
    0x00000000, 0x45888C8C, 0xFF637E8B, 0xFF607D8B, 0xFF607D8B, 0xFF607D8B, 0xFF678390, 0xFF78909C, 0xFF78909C, 0xFF66828F, 0xFF607D8B, 0xFF607D8B, 0xFF607D8B, 0xFF647E8B, 0x438C8C8C, 0x00000000,
    0x00000000, 0x755F7C8B, 0xFF607D8B, 0xFF425660, 0xFF5E7E12, 0xFF546F36, 0xFF607D8B, 0xFF758D9A, 0xFF748D9A, 0xFF607D8B, 0xFF547035, 0xFF5E7E12, 0xFF425660, 0xFF607D8B, 0x735F7C8B, 0x00000000,
    0x00000000, 0x9E5F7C8A, 0xFF607D8B, 0xFF35454D, 0xFF6B9000, 0xFF638600, 0xFF526B77, 0xFF718B97, 0xFF718B97, 0xFF526A76, 0xFF638600, 0xFF6A8F00, 0xFF35454D, 0xFF607D8B, 0x9D5F7D8B, 0x00000000,
    0x00000000, 0x99607D8A, 0xFF607D8B, 0xFF5E7B88, 0xFF303F46, 0xFF263238, 0xFF546D79, 0xFF728B98, 0xFF728B98, 0xFF546D79, 0xFF263238, 0xFF303F46, 0xFF5E7B88, 0xFF607D8B, 0x985F7D8B, 0x00000000,
    0x00000000, 0x5D5F7E8B, 0xFF607D8B, 0xFF607D8B, 0xFF607D8B, 0xFF6A8592, 0xFFA7B8BF, 0xFF64767F, 0xFF637680, 0xFFA7B7BF, 0xFF6A8592, 0xFF607D8B, 0xFF607D8B, 0xFF607D8B, 0x5B5F7E8C, 0x00000000,
    0x00000000, 0x06557F7F, 0xD05F7D8A, 0xFF607D8B, 0xFF607D8B, 0xFFB2C0C7, 0xFFCFD8DC, 0xFFA2B2BA, 0xFFA2B3BA, 0xFFCFD8DC, 0xFFB1C0C6, 0xFF607D8B, 0xFF607D8B, 0xCD5F7C8B, 0x06557F7F, 0x00000000,
    0x00000000, 0x00000000, 0x1D607B8C, 0xD25F7D8A, 0xFF607D8B, 0xFFC7D2D6, 0xFFCFD8DC, 0xFFAABAC1, 0xFFAABAC1, 0xFFCFD8DC, 0xFFC7D1D6, 0xFF607D8B, 0xD1607C8B, 0x1B5E7A8D, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x085F7F7F, 0x765F7D8A, 0xF19BAEB7, 0xFFA7B7BF, 0xFF7A929D, 0xFF7A919D, 0xFFA8B8C0, 0xF09CAEB6, 0x755F7C8B, 0x076D6D91, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xAD94A8AF, 0xE2B1BEC4, 0xE2B0BDC4, 0xAC94A7AE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000
};



static void* cat()
{
    printf(" %I%I\n\n",cat_icon1,cat_icon2);

    return 0;
}


static void py(char diskletter, char* name)
{
    elf_load('A',"./.py.elf",(i_ptr)diskletter,(i_ptr)name);
}

INIT void init(void* (*_resolve_function)(char* name))
{
    api_init(_resolve_function);

    register_function("fetch",fetch,"print short system information");
    register_function("reboot",reboot,"reboot system now");
    register_function("date",date,"current date and time");
    register_function("lsmod",lsmod,"print all loaded modules");
    register_function("cls",cls,"clear screen");
    register_function("c",cls,"clear screen");
    register_function("ps",ps,"proccess list");
    register_function("lsblk",lsblk,"print all disk");
    register_function("lsrd",printramdisk,"print ramdisk");
    register_function("cat",cat,"print cat");

    register_function("_pf",printfile,"print file. use _pf;c:diskletter;name");
    register_function("_wf",writefile,"write file. use _pf;c:diskletter;name");
    register_function("_ls",ls,"print directory. use _ls;c:diskletter;name");
    register_function("_dmp",dumpmem,"hexdump ram. _dmp;i:0xaddr;i:size");
    register_function("_dmpdsk",dumpdisk,"hexdump disk. _dmpdsk;i:diskidx;i:lba;i:blocks");
    register_function("_elfload",elf_load,"_elfload(char diskletter,char* name) -> bool success");
    if(!_resolve_function("_img")) register_function("_img",img,"_img;c:diskletter;name. view image");

    struct file* pyelf = open('A',"./.py.elf");
    if(pyelf) {
        register_function("_py",py,"_py(char disk_letter, char* name) -> None");
        pyelf->close(pyelf);
    }

    create_task("clitools thread",clitools);
}
