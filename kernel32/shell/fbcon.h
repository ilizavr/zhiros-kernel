#include "../../lib/zhirtypes.h"
#include "../printf.h"
#include "../../lib/string.h"

extern void fbcon_resume();
extern void init_fbcon(i_ptr _fb_addr, u32 _screen_width, u32 _screen_height, u32 _screen_pitch, u32 _bbp);
extern void clearframe();
#define CLS clearframe
extern void putchar_color(char chr, u32 color);
extern void putchar(char chr);
extern API void print_color(char *str, u32 color);
extern void print(char *str);
extern void print_hex(u32 num);
extern void print_int(u32 num);
extern API struct fb_info* fbcon_stop();
extern API struct fb_info* getfb();

