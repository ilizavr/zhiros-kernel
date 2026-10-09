/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */
//код написан https://github.com/TINERKOTL для теста
#include "../lib/zhirtypes.h"
#include "../lib/string.h"
u32 (*__getchar)();
void (*__printf)(char* fmt, ...);
void (*__clearframe)();
bool *(*__get_key_state_matrix)();

struct fb_info* (*__getfb)();

struct fb_info* fb;

void put_pixel(u32 x, u32 y, u32 color) {
    volatile u8 *pixel_addr = (volatile u8*)(fb->fb_addr + y * fb->screen_pitch + x * fb->bpp);

    *(volatile u32*)pixel_addr = color;
}

void draw_line(u32 startx, u32 endx, u32 y, u32 color) {
    for (u32 x = startx; x < endx; x++)
        put_pixel(x, y, color);
}

void draw_cube(u32 startx, u32 endx, u32 starty, u32 endy, u32 color) {
    for (u32 x = startx; x < endx; x++)
        for (u32 y = starty; y < endy; y++)
            put_pixel(x, y, color);
}

bool collision(u32 cube_start, u32 cube_end, u32 platform_start, u32 platform_end) {
    return cube_end >= platform_start && cube_start <= platform_end;
}

// u32 x = 100;

INIT void init(void* _resolve_function(char* name))
{
    __getchar = _resolve_function("_getchar");
    __printf = _resolve_function("_printf");
    __getfb = _resolve_function("_getfb");
    __clearframe = _resolve_function("_clearframe");
    __get_key_state_matrix = _resolve_function("_get_key_state_matrix");
    bool *key_state_matrix = __get_key_state_matrix();

    u8 wcode_a = ascii_to_keycode['a'];
    u8 wcode_d = ascii_to_keycode['d'];

    u8 wcode_j = ascii_to_keycode['j'];
    u8 wcode_l = ascii_to_keycode['l'];

    fb = __getfb();

    __clearframe();

    u32 x = 200, x_2 = 200, end_x = 350, end_x_2 = 350;
    u32 cube_x = 400, cube_y = 300, cube_size = 15;
    u32 top_y = 53, bottom_y = 547;
    u32 platform_center, cube_center;
    bool cube_direction_y = 1; // 1 - вверх, 0 - вниз
    bool cube_direction_x = 1; // 1 - влево, 0 - вправо
    bool game_over = 0;

    draw_line(x,x+150, fb->screen_height - 50, 0xFFFFFF);
    draw_line(x_2,x_2+150, 50, 0xFFFFFF);


    while (game_over == 0) {

        if(key_state_matrix[wcode_a]) 
            if (x > 0){
                draw_line(x, end_x, fb->screen_height - 50, 0x000000);
                x -= 2;
                end_x -= 2;
                draw_line(x, end_x, fb->screen_height - 50, 0xFFFFFF);
            }


        if(key_state_matrix[wcode_d]) 
            if (x < fb->screen_width - 150){
                draw_line(x, end_x, fb->screen_height - 50, 0x000000);
                x += 2;
                end_x += 2;
                draw_line(x, end_x, fb->screen_height - 50, 0xFFFFFF);
            }


        if(key_state_matrix[wcode_j]) 
            if (x_2 > 0){
                draw_line(x_2, end_x_2, 50, 0x000000);
                x_2 -= 2;
                end_x_2 -= 2;
                draw_line(x_2, end_x_2, 50, 0xFFFFFF);
            }

        if(key_state_matrix[wcode_l]) 
            if (x_2 < fb->screen_width - 150){
                draw_line(x_2, end_x_2, 50, 0x000000);
                x_2 += 2;
                end_x_2 += 2;
                draw_line(x_2, end_x_2, 50, 0xFFFFFF);
            }

        draw_cube(cube_x, cube_x+cube_size, cube_y, cube_y+cube_size, 0x000000);

        bool hit_top = collision(cube_x, cube_x + cube_size, x_2, end_x_2) && cube_y <= top_y;
        bool hit_bottom = collision(cube_x, cube_x + cube_size, x, end_x) && cube_y + cube_size >= bottom_y;


        if (cube_direction_y == 1)
        {
            if (hit_top) {
                cube_direction_y = 0;
                cube_y = top_y + cube_size;

                platform_center = (x_2 + end_x_2)/2;
                cube_center = cube_x + cube_size / 2;

                if (cube_center > platform_center)
                    cube_direction_x = 0;
                else
                    cube_direction_x = 1;
            }
            else if (cube_y > 0) {
                cube_y--;
            }
            else {
                game_over = 1;
            }
        } else {
            if (hit_bottom) {
                cube_direction_y = 1;

                cube_y = bottom_y - cube_size * 2;

                platform_center = (end_x + x)/2;
                cube_center = cube_x + cube_size / 2;

                if (cube_center < platform_center)
                    cube_direction_x = 1;
                else
                    cube_direction_x = 0;
            }
            else if (cube_y + cube_size < fb->screen_height) {
                cube_y++;
            }
            else {
                game_over = 1;
            }
        }


        if (cube_direction_x == 1) {
            if (cube_x > 0)
                cube_x--;
            else
                cube_direction_x = 0;
        } else {
            if (cube_x + cube_size < fb->screen_width)
                cube_x++;
            else
                cube_direction_x = 1;
        }
        draw_cube(cube_x, cube_x+cube_size, cube_y, cube_y+cube_size, 0xFFFFFF);
        
        for (u32 i = 0; i < 1500000; i++);
    }

    __clearframe();

    __printf("GAME OVER");
}
