#ifndef ZHIRAPI_H
#define ZHIRAPI_H

#include "zhirtypes.h"

void (*print_color)(char *string, u32 color);
void (*printf)(...);
void (*putchar_color)(char chr, u32 color);
void (*clearframe)();
struct fb_info* (*fbcon_stop)();
struct fb_info* (*getfb)();
void (*set_font_scale)(u32 scale);

bool* (*get_key_state_matrix)();
int (*gets)(char* string, int max_size);
char (*getchar)();

void* (*alloc)(u32 size);
bool (*free)(void* buf);
i_ptr (*getfree)();
i_ptr (*getused)();
struct memoryblock* (*get_first_block)();

struct function_info* (*get_linker_head)();
struct module_info* (*get_module_array)();
void* (*resolve_function)(char* name);
void (*register_function)(char *function_name, void* call, char *description);
bool (*replace_function)(char *function_name, void* newfnc);
bool (*hook_interrupt)(u32 n, void* function);
bool (*load_mod)(char diskletter, char* path);
bool (*unload_mod)(u32 idx);

void (*sleep_ms)(u32 ms);
u32 (*getticks)();

bool (*diskadd)(struct disk* dsk);
struct disk* (*getdisk)(int idx);

bool (*mount)(struct disk* dsk, void * open_fnc, void* mkdir_function, void* create_file_function);
struct file* (*open)(char diskletter, char* path);
struct file* (*create_file)(char diskletter, char* path);
struct file* (*mkdir)(char diskletter, char* path);

void (*yield)();
struct task* (*get_tasks)();
struct task* (*create_task)(char * name, void* function);
void (*kill)(u32 tid);

void (*exec_line)(char* cmd);
u16 (*get_kernel_checksum)();

static inline void api_init(void* (*_resolve_function)(char* name)) {
    if (!_resolve_function) return;

    resolve_function = _resolve_function;

    print_color = _resolve_function("_print_color");
    putchar_color = _resolve_function("_putchar_color");
    printf = _resolve_function("_printf");
    clearframe = _resolve_function("_clearframe");
    fbcon_stop = _resolve_function("_fbcon_stop");
    getfb = _resolve_function("_getfb");
    set_font_scale = _resolve_function("_set_font_scale");

    get_key_state_matrix = _resolve_function("_get_key_state_matrix");
    gets = _resolve_function("_gets");
    getchar = _resolve_function("_getchar");

    alloc = _resolve_function("_alloc");
    free = _resolve_function("_free");
    getfree = _resolve_function("_getfree");
    getused = _resolve_function("_getused");
    get_first_block = _resolve_function("_get_first_block");

    get_linker_head = _resolve_function("_get_linker_head");
    get_module_array = _resolve_function("_get_module_array");
    register_function = _resolve_function("_register_function");
    replace_function = _resolve_function("_replace_function");
    hook_interrupt = _resolve_function("_hook_interrupt");
    load_mod = _resolve_function("_load_mod");
    unload_mod = _resolve_function("_unload_mod");

    sleep_ms = _resolve_function("_sleep_ms");
    getticks = _resolve_function("_getticks");

    diskadd = _resolve_function("_diskadd");
    getdisk = _resolve_function("_getdisk");

    mount = _resolve_function("_mount");
    open = _resolve_function("_open");
    mkdir = _resolve_function("_mkdir");
    create_file = _resolve_function("_create_file");

    yield = _resolve_function("_yield");
    get_tasks = _resolve_function("_get_tasks");
    create_task = _resolve_function("_create_task");
    kill = _resolve_function("_kill");

    exec_line = _resolve_function("_exec_line");
    get_kernel_checksum = _resolve_function("_get_kernel_checksum");
}

#endif
