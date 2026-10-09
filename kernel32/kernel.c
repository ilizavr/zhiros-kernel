/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../lib/zhirtypes.h"
#include "multiboot.h"
#include "shell/fbcon.h"
#include "panic.h"
#include "interrupt.h"
#include "allocator.h"
#include "disk.h"
#include "linker.h"
#include "shell/keyboard.h"
#include "vfs.h"
#include "shell/shell.h"
#include "timer.h"

void main(i_ptr addr, u32 magic)
{
    if(magic != MULTIBOOT2_BOOTLOADER_MAGIC) return;

    struct multiboot_tag_framebuffer_common *fb_tag = 0;
    struct multiboot_tag_mmap *mmap_tag = 0;
    struct multiboot_tag_module *module_tag = 0;

    struct multiboot_tag* tag = (struct multiboot_tag*)(addr+8);

    for(;tag->type != MULTIBOOT_TAG_TYPE_END;
        tag = (struct multiboot_tag*)((i_ptr)tag+((tag->size+7)&~7))
    )
    {
        switch (tag->type)
        {
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                fb_tag = (void*)tag;
                break;
            case MULTIBOOT_TAG_TYPE_MMAP:
                mmap_tag = (void*)tag;
                break;
            case MULTIBOOT_TAG_TYPE_MODULE:
                module_tag = (void*)tag;
                break;
        }
    }

    if(!fb_tag) return;

    init_fbcon(fb_tag->framebuffer_addr, fb_tag->framebuffer_width, fb_tag->framebuffer_height, fb_tag->framebuffer_pitch, fb_tag->framebuffer_bpp);
    LOGI("fbcon inited");

    if(!module_tag)
    {
        kernel_panic_text("Multiboot not give module!");
    }
    if(!mmap_tag)
    {
        kernel_panic_text("Multiboot not give mmap!");
    }

    LOGI("module loaded at 0x%x-0x%x",module_tag->mod_start,module_tag->mod_end);

    pic_remap();
    init_idt();
    STI();
    LOGI("interrupt inited");


    struct multiboot_mmap_entry *entry = mmap_tag->entries;
    struct multiboot_mmap_entry *end = (void*)mmap_tag + mmap_tag->size;
    while (entry < end) {
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && entry->len > 0 && entry->addr+entry->len <= 0x100000000) {
            allocator_add_page(entry->addr,entry->addr+entry->len,module_tag->mod_start,module_tag->mod_end);
        }
        entry = (void*)entry + mmap_tag->entry_size;
    }
    LOGI("allocator inited. memory available: %uM",get_available_memory()>>20);

    struct disk* ramdisk = ramdisk_init(module_tag->mod_start,module_tag->mod_end);

    init_keyboard();
    LOGI("keyboard inited");
    init_timer();
    LOGI("timer inited");

    //fastcall api
    register_function("_resolve_function",resolve_function,"_resolve_function(char* name) -> void* function");
    register_function("_register_function",register_function,"_register_function(char *function_name, void* call, char *description) -> None");
    register_function("_replace_function",replace_function,"_replace_function(char *function_name, void* newfnc) -> bool success");
    register_function("_printf",printf,"_printf(...) -> None");
    register_function("_clearframe",clearframe,"_clearframe() -> None");
    register_function("_gets",gets,"_gets(char* string, u32 max_size) -> u32 readed");
    register_function("_getchar",getchar,"_getchar() -> char sym");
    register_function("_get_key_state_matrix",get_key_state_matrix,"_get_key_state_matrix() -> bool key_state_matrix[256]");
    register_function("_print_color",print_color,"_print_color(char *string, u32 color) -> None");
    register_function("_alloc",kalloc,"_alloc(u32 size) -> void* buf");
    register_function("_free",free,"_free(void* buf) -> bool success");
    register_function("_getfree",get_available_memory,"_getfree() -> i_ptr freemem");
    register_function("_hook_interrupt",hook_interrupt,"_hook_interrupt(u32 n, void* function) -> bool success");
    register_function("_diskadd",diskadd,"_diskadd(struct disk* dsk) -> bool success");
    register_function("_getdisk",getdisk,"_getdisk(int idx) -> struct disk*");
    register_function("_fbcon_stop",fbcon_stop,"_fbcon_stop() -> struct fb_info*");
    register_function("_getfb",getfb,"_getfb() -> struct fb_info*");
    register_function("_mount",mount,"_mount(struct disk* dsk, void * open_fnc, void* mkdir_fnc) -> char diskletter");
    register_function("_open",open,"_open(char diskletter, char* path) -> struct file*");
    register_function("_mkdir",open,"_mkdir(char diskletter, char* path) -> struct file*");
    register_function("_load_mod",load_mod,"_load_mod(char diskletter, char* path) -> bool success");
    register_function("_unload_mod",unload_mod,"_unload_mod(u32 idx) -> bool success");
    register_function("_get_linker_head",get_linker_head,"_get_linker_head() -> struct function_info*");
    register_function("_get_module_array",get_module_array,"_get_module_array() -> struct module_info*");
    register_function("_sleep_ms",sleep_ms,"_sleep_ms(u32 ms) -> None");
    register_function("_getticks",getticks,"_getticks() -> u32 ticks");
    LOGI("kernel api registred");

    char ramdisk_letter = mount(ramdisk,open_unstar,0);
    LOGI("ramdisk letter: %c",ramdisk_letter);

    //char buf[256];gets(buf,256);

#include "../init.h"

}
