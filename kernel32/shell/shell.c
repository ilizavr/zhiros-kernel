#include "../../lib/zhirtypes.h"
#include "../../lib/string.h"
#include "../linker.h"
#include "fbcon.h"
#include "keyboard.h"
#include "../printf.h"
#include "../allocator.h"
#include "../../lib/hexdump.h"
#include "../vfs.h"
#include "../multitask.h"
#include "../disk.h"

#define MAX_ARGS 4

static char *strdup(char *str)
{
    u32 len = strlen(str);
    char * newstr = kalloc(len+1);
    memcpy(newstr,str,len+1);
    return newstr;
}

void* printlinkerlist()
{
    struct function_info * current = get_linker_head();
    while(current)
    {
        if(current->name[0] == '_') {current=current->next; continue;}
        print_color(current->name,0xAAAAFF);
        for(int i = strlen(current->name);i<32;i++)putchar(' ');
        print_color(current->description,0x00FFFF);
        putchar('\n');

        current=current->next;
    }
    putchar('\n');

    current = get_linker_head();
    while(current)
    {
        if(current->name[0] != '_') {current=current->next; continue;}
        print_color(current->name,0x00FFAA);
        for(int i = strlen(current->name);i<32;i++)putchar(' ');
        print_color(current->description,0x00FFFF);
        putchar('\n');

        current=current->next;
    }
    return 0;
}

void dumpmem(void* addr, i_ptr size)
{
    if(size>65536)
    {
        LOGW("memory block is big. continue?");
        char chr = getchar();
        if(chr!='y') return;
    }
    hexdump(addr,size);
}
void dumpdisk(u32 idx, u32 lba, u32 blocks)
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

    void *buf = kalloc(blocks*512);
    d->lba_read(d,lba,buf,blocks);

    hexdump(buf,blocks*512);
    free(buf);
}

void* cls()
{
    clearframe();
    return 0;
}

void* lsmod()
{
    struct module_info * mdls = get_module_array();
    for(int i = 0; i< 256;i++) if(mdls[i].start) printf("%u. %s | %x-%x\n",i,mdls[i].path, mdls[i].start,mdls[i].start+mdls[i].size);

    return 0;
}
void printfile(char diskletter, char* name)
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
    char *buffer = kalloc(size+1);
    f->read(f,buffer,size,0);
    buffer[size] = 0;
    printf("%s",buffer);
    f->close(f);
    free(buffer);
}

void* ps()
{
    struct task* tasks = get_tasks();
    printf("TID | NAME\n");
    for(int i = 0;i<MAX_TASKS;i++)
    {
        if(tasks[i].tid!=0) printf("%u    %s\n",tasks[i].tid,tasks[i].name);
    }
    return NULL;
}

void* lsblk()
{
    for(int i = 0; i< MAX_DISK_COUNT;i++)
    {
        struct disk* d = getdisk(i);
        if(!d) continue;

        printf("index:%u    name:%s    size:%ukb",i,d->name,d->size/2);
        if(d->mountletter) printf("    letter:%c",d->mountletter);
        printf("\n");
    }

    return 0;
}

void start_shell()
{
    register_function("help",printlinkerlist,"print all functions in linker list");
    register_function("lsmod",lsmod,"print all loaded modules");
    register_function("lsblk",lsblk,"print all disk");
    register_function("cls",cls,"clear screen");
    register_function("ps",ps,"proccess list");
    register_function("_pf",printfile,"print file. use _pf;c:diskletter;name");
    register_function("_dmp",dumpmem,"hexdump ram. _dmp;i:0xaddr;i:size");
    register_function("_dmpdsk",dumpdisk,"hexdump disk. _dmpdsk;i:diskidx;i:lba;i:blocks");


    printf("debug shell\n");
    while(true)
    {
        i_ptr args[MAX_ARGS];
        u32 current_arg = 0;

        char buffer[256];
        printf(">");
        gets(buffer,256);

        if(!strlen(buffer)){
            continue;
        }

        char d[] = ";";
        char * token = strtok(buffer,d);


        if(token[0] == '*')
        {
            i_ptr (*_function)(...) = (void*)strtol(token+1,0,0);
            token = strtok(0,d);

            while(token&&current_arg<4)
            {
                if(token[0] == 'i'&&token[1] == ':') args[current_arg] = strtol(token+2,0,0);
                else if(token[0] == 'c'&&token[1] == ':') args[current_arg] = token[2];
                else args[current_arg] = (i_ptr)token;

                token = strtok(0,d);
                current_arg++;
            }

            i_ptr ret = _function(args[0],args[1],args[2],args[3]);
            printf("\nreturn %x\n",ret);
        }
        else if (token[0] == '_'){ // fastcall
            i_ptr (*_function)(...) = resolve_function(token);

            if(!_function) {
                LOGE("function %s not found!",token);
                continue;
            }

            token = strtok(0,d);

            while(token&&current_arg<4)
            {
                if(token[0] == 'i'&&token[1] == ':') args[current_arg] = strtol(token+2,0,0);
                else if(token[0] == 'c'&&token[1] == ':') args[current_arg] = token[2];
                else args[current_arg] = (i_ptr)token;

                token = strtok(0,d);
                current_arg++;
            }

            i_ptr ret = _function(args[0],args[1],args[2],args[3]);
            printf("\nreturn %x\n",ret);
        }
        else { // zhirfunction
            i_ptr (*_function)(...) = resolve_function(token);

            if(!_function) {
                LOGE("function %s not found!",token);
                continue;
            }
            //TODO: дописать обработку аргументов zhirfunction

            _function();
        }
    }
}
