#include "../lib/zhirtypes.h"
#include "../lib/string.h"
#include "../lib/ports.h"
#include "../lib/zhirapi.h"


static struct __attribute__((packed))
{
    u16 bios_lba;
    u16 reserved;
    struct dap_t *dap;
}* bios_asm_header;

struct __attribute__((packed)) dap_t
{
    u8 size;
    u8 res;
    u16 count;
    u16 offset;
    u16 segment;
    u32 lba;
    u32 lba2;

    u8 buffer[0];
};

static bool bios_read_lba(struct disk* dsk, u32 lba, char* buffer, u32 blocks)
{
    bios_asm_header->dap->size = 0x10;
    bios_asm_header->dap->res = 0;
    bios_asm_header->dap->count = blocks;
    bios_asm_header->dap->offset = (u16)bios_asm_header->dap->buffer;
    bios_asm_header->dap->segment = 0;
    bios_asm_header->dap->lba = lba;
    bios_asm_header->dap->lba2 = 0;

    outb(0x21,0xFF);//mask pic interrupt
    outb(0xA1,0xFF);

    CALL(bios_asm_header->bios_lba,0x42,dsk->other_info);

    outb(0x21,0);//enable pic
    outb(0xA1,0);
    memcpy(buffer,bios_asm_header->dap->buffer,blocks*512);
}

INIT void init(void* (*_resolve_function)(char* name))
{
    bool mbrscan = false;

    api_init(_resolve_function);

    LOGW("bios disk driver is unstable. continue loading?(y/n)");
    char key;
    while(key=getchar()){
        if(key == 'y') break;
        if(key == 'n') return;
    }
    LOGW("use auto mbrscan?(y/n)");
    while(key=getchar()){
        if(key == 'y') { mbrscan=true; break; }
        if(key == 'n') break;
    }

    bios_asm_header = (void*)0x7000;
    struct file* bios = open('A',"./bios.bin");
    bios->read(bios,bios_asm_header,bios->getsize(bios),0);

    u8 disks_count = *(u8*)0x475;

    for(int i = 0;i<disks_count;i++)
    {
        NEW(struct disk, n);
        n->name = "bios";
        n->other_info = (void*)(i+0x80);
        n->lba_read = bios_read_lba;


        outb(0x21,0xFF);//mask pic interrupt
        outb(0xA1,0xFF);

        *(u16*)bios_asm_header->dap = 30;
        CALL(bios_asm_header->bios_lba,0x48,0x80+i);
        n->size = *(u32*)((u32)bios_asm_header->dap+0x10);

        outb(0x21,0);//enable pic
        outb(0xA1,0);

        if(mbrscan) APICALL("_mbrscan",n);
        diskadd(n);
    }
    LOGI("disk driver inited. found %u disk",disks_count);
}
