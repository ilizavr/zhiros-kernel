/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */

#include "../lib/zhirtypes.h"
#include "../lib/string.h"
#include "../lib/ports.h"

bool (*hook_interrupt)(u32 n, void* function);
void (*register_function)(char *function_name, void* call, char *description);
void (*printf)(char *fmt,...);
void (*print_color)(char *str, u32 color);

// type == 0: ждем, пока входной буфер освободится (можно писать в 0x64 / 0x60)
// type == 1: ждем, пока выходной буфер заполнится (есть данные для чтения из 0x60)
void mouse_wait(u8 type) {
    u32 timeout = 100000;
    if (type == 0) {
        while ((inb(0x64) & 2) && timeout--);
    } else {
        while (!(inb(0x64) & 1) && timeout--);
    }
}

void mouse_write(u8 data) {
    mouse_wait(0); // Ждем готовности контроллера к приему команды
    outb(0x64, 0xD4);
    mouse_wait(0); // Ждем готовности контроллера к приему данных мыши
    outb(0x60, data);
}

u8 mouse_read() {
    u32 timeout = 100000;
    while (timeout--) {
        u8 status = inb(0x64);
        if (status & 1) { // Проверяем наличие данных в выходном буфере (Bit 0)
            return inb(0x60);
        }
    }
    return 0; // Возвращаем 0 при таймауте во избежание зависания
}

void init_mouse(void) {
    u8 status;

    // Отключаем прерывания на время инициализации во избежание гонок
    asm volatile("cli");

    // 1. Включаем вспомогательное устройство (PS/2 Mouse) в контроллере 8042
    // Перед записью в 0x64 ждем освобождения входного буфера -> mouse_wait(0)
    mouse_wait(0);
    outb(0x64, 0xA8);

    // 2. Включаем прерывание IRQ12 в Command Byte контроллера
    mouse_wait(0);
    outb(0x64, 0x20); // Команда 0x20: прочитать Command Byte

    mouse_wait(1);    // Для чтения из 0x60 ждем заполнения выходного буфера -> mouse_wait(1)
    status = (inb(0x60) | 2); // Устанавливаем Бит 1 (Enable IRQ12)

    mouse_wait(0);
    outb(0x64, 0x60); // Команда 0x60: записать Command Byte

    mouse_wait(0);
    outb(0x60, status);

    // 3. Настройка самой мыши (Включение передачи пакетов)
    mouse_write(0xF4); // Команда 0xF4: Enable Data Reporting
    mouse_read();      // Считываем ACK (0xFA), отправленный мышью

    // 4. Размаскируем IRQ12 на Slave PIC (порт 0xA1, 4-й бит = IRQ12)
    u8 mask = inb(0xA1);
    outb(0xA1, mask & ~(1 << 4));

    // Возвращаем прерывания
    asm volatile("sti");
}

volatile u8 mouse_cycle = 0;
volatile u8 mouse_packet[3];

volatile s32 mouse_x = 100;
volatile s32 mouse_y = 100;
volatile u8 mouse_left = 0;
volatile u8 mouse_right = 0;

static void mouse_irq_handler() {
    u8 data = inb(0x60);

    if (mouse_cycle == 0 && !(data & 0x08)) {
        return;
    }

    mouse_packet[mouse_cycle] = data;
    mouse_cycle++;

    if (mouse_cycle == 3) {
        mouse_cycle = 0;

        mouse_left  = mouse_packet[0] & 0x01;
        mouse_right = mouse_packet[0] & 0x02;

        s16 dx = mouse_packet[1];
        s16 dy = mouse_packet[2];

        if (mouse_packet[0] & 0x10) dx |= 0xFF00;
        if (mouse_packet[0] & 0x20) dy |= 0xFF00;

        mouse_x += dx;
        mouse_y -= dy;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x >= 800) mouse_x = 799;
        if (mouse_y >= 600) mouse_y = 599;
    }
}

static s32 get_mouse_x() { return mouse_x; }
static s32 get_mouse_y() { return mouse_y; }
static bool get_mouse_left() { return mouse_left; }
static bool get_mouse_right() { return mouse_right; }

INIT void init(void* _resolve_function(char* name))
{
    printf = _resolve_function("_printf");
    print_color = _resolve_function("_print_color");
    register_function = _resolve_function("_register_function");
    hook_interrupt = _resolve_function("_hook_interrupt");

    if(!register_function||!hook_interrupt) LOGE("kernel verion is unsupported by mouse module");

    init_mouse();

    if(!hook_interrupt(0x2C, mouse_irq_handler))LOGE("error hook interrupt");
    register_function("_get_mouse_x",get_mouse_x,"_get_mouse_x() -> s32");
    register_function("_get_mouse_y",get_mouse_y,"_get_mouse_y() -> s32");
    register_function("_get_mouse_left",get_mouse_left,"_get_mouse_left() -> bool");
    register_function("_get_mouse_right",get_mouse_right,"_get_mouse_right() -> bool");

    LOGI("mouse inited");
}
