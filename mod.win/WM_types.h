#include "../lib/zhirtypes.h"
#include "../lib/string.h"
#include "../fonts/font8x16.h"

typedef struct Window Window;

typedef void (*WindowResizeCallback)(Window *window, s32 old_width, s32 old_height);
typedef void (*DesktopIconDraw)(u32 x, u32 y, u32 size);

struct Window{
    s32 x,y;
    bool window_on_changed;

    s32 width, height;
    bool invisible;
    u32 *buffer, buffer_size;
    bool old_keys[256];

    char *name;

    s32 prev_x, prev_y, prev_width, prev_height;
    bool full_screen;
    WindowResizeCallback on_resize;

    struct task *task;
};

typedef struct DirtyRect {
    s32 x,y,w,h;
    bool valid;
} DirtyRect;

struct image{
    u16 width;
    u16 height;
    u32 bytes[0];
};

typedef struct {
    u32 x;
    u32 y;
    u32 size;
    DesktopIconDraw draw;
} DesktopIcon;
