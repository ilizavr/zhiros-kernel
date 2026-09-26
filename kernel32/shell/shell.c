#include "../../lib/zhirtypes.h"
#include "../../lib/string.h"
#include "../linker.h"
#include "fbcon.h"
#include "keyboard.h"
#include "../printf.h"
#include "../allocator.h"


#define MAX_ARGS 4

static char *strdup(char *str)
{
    u32 len = strlen(str);
    char * newstr = kalloc(len+1);
    memcpy(newstr,str,len+1);
    return newstr;
}

void start_shell()
{
    printf("shell started\nuse _function type:arg1 string_arg2 ...\n");
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
