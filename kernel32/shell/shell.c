#include "../../lib/zhirtypes.h"
#include "../../lib/string.h"
#include "../linker.h"
#include "fbcon.h"
#include "keyboard.h"
#include "../printf.h"
#include "../allocator.h"
#include "../../lib/hexdump.h"


#define MAX_ARGS 4

static char *strdup(char *str)
{
    u32 len = strlen(str);
    char * newstr = kalloc(len+1);
    memcpy(newstr,str,len+1);
    return newstr;
}

void printlinkerlist()
{
    struct function_info * current = get_linker_head();
    while(current)
    {
        if(current->name[0] == '_') print_color(current->name,0x88FF55);
        else print_color(current->name,0xFFFFFF);
        for(int i = strlen(current->name);i<32;i++)putchar(' ');
        print_color(current->description,0x00FFFF);
        putchar('\n');

        current=current->next;
    }
}

void dumpmem(char* type, void* addr, i_ptr size)
{
    if(!strcmp(type,"hd")||!strcmp(type,"hexdump"))
    {
        hexdump(addr,size);
    }
    if(!strcmp(type,"file"))
    {
        struct file* fi = addr;
        printf("is_dir = %u\nread = %x\nwrite = %x\ngetsize = %x\nclose = %x",fi->is_dir, fi->read,fi->write,fi->getsize,fi->close);
    }
    if(!strcmp(type,"help"))
    {
        printf("use _mem type i:<addr> [i:size]\n");
        printf("types:\nhd - hexdump\nfile - file struct dump");
    }
}

void lsmod()
{
    struct module_info * mdls = get_module_array();
    for(int i = 0; i< 256;i++) if(mdls[i].start) printf("%s | %x-%x",mdls[i].path, mdls[i].start,mdls[i].start+mdls[i].size);
}

void start_shell()
{
    register_function("_fncs",printlinkerlist,"print linker list");
    register_function("_mem",dumpmem,"use _mem help");
    register_function("_lsmod",lsmod,"");


    printf("shell started\ntype _fncs to get all linker funtion\n");
    printf("  use _function type:arg1 type:arg2\n");
    printf("  if arg is string _function stringarg\n");
    printf("  use *0xaddr to call function by address\n");
    while(true)
    {
        i_ptr args[MAX_ARGS];
        u32 current_arg = 0;

        char buffer[256];
        print_color(">",0xFFFFFF);
        gets(buffer,256);

        if(!strlen(buffer)){
            continue;
        }

        char d[] = " \t\n";
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
        else {
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
    }
}
