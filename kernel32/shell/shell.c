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

struct object * printlinkerlist(struct objectArray* args)
{
    struct function_info * current = get_linker_head();
    while(current)
    {
        if(current->name[0] == '_') {current=current->next; continue;}
        print_color(current->name,0xAAAAFF);
        for(int i = strlen(current->name);i<32;i++)printf(" ");
        print_color(current->description,0x00FFFF);
        printf("\n");

        current=current->next;
    }
    printf("\n");

    current = get_linker_head();
    while(current)
    {
        if(current->name[0] != '_') {current=current->next; continue;}
        print_color(current->name,0x00FFAA);
        for(int i = strlen(current->name);i<32;i++)printf(" ");
        print_color(current->description,0x00FFFF);
        printf("\n");

        current=current->next;
    }
    return 0;
}

void (*custom_gets)(char * buf, u32 maxlen) = 0;

void exec_line(char * buffer)
{
    i_ptr args[MAX_ARGS];
    u32 current_arg = 0;

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
            return;
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
            return;
        }
        //TODO: дописать обработку аргументов zhirfunction

        _function();
    }
}

void start_shell()
{
    register_function("help",printlinkerlist,"print all functions in linker list");

    printf("debug shell\n");
    while(true)
    {
        if(!custom_gets) custom_gets = resolve_function("_custom_gets");



        char buffer[256];
        print_color("\x1A",0xFF0000);
        if(custom_gets) custom_gets(buffer, 256);
        else gets(buffer, 256);

        if(!strlen(buffer)){
            continue;
        }

        exec_line(buffer);

    }
}
