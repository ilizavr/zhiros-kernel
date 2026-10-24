#ifndef ZHIRAPI_H
#define ZHIRAPI_H

#include "zhirtypes.h"

static void (*print_color)(char *string, u32 color);
static void (*printf)(...);
static void (*putchar_color)(char chr, u32 color);
static void (*clearframe)();
static struct fb_info* (*fbcon_stop)();
static struct fb_info* (*getfb)();
static void (*set_font_scale)(u32 scale);

static bool* (*get_key_state_matrix)();
static int (*gets)(char* string, int max_size);
static char (*getchar)();

static void* (*alloc)(u32 size);
static bool (*free)(void* buf);
static i_ptr (*getfree)();
static i_ptr (*getused)();

static struct function_info* (*get_linker_head)();
static struct module_info* (*get_module_array)();
static void* (*resolve_function)(char* name);
static void (*register_function)(char *function_name, void* call, char *description);
static bool (*replace_function)(char *function_name, void* newfnc);
static bool (*hook_interrupt)(u32 n, void* function);
static bool (*load_mod)(char diskletter, char* path);
static bool (*unload_mod)(u32 idx);

static void (*sleep_ms)(u32 ms);
static u32 (*getticks)();

static bool (*diskadd)(struct disk* dsk);
static struct disk* (*getdisk)(int idx);

static bool (*mount)(struct disk* dsk, void * open_fnc, void* mkdir_function);
static struct file* (*open)(char diskletter, char* path);
static struct file* (*mkdir)(char diskletter, char* path);

static void (*yield)();
static struct task* (*get_tasks)();
static struct task* (*create_task)(char * name, void* function);

static void (*exec_line)(char* cmd);

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

    yield = _resolve_function("_yield");
    get_tasks = _resolve_function("_get_tasks");
    create_task = _resolve_function("_create_task");

    exec_line = _resolve_function("_exec_line");
}

#endif
