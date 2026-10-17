bits 32

MULTIBOOT2_HEADER_MAGIC equ 0xe85250d6

section .multiboot2
 align 8

header_start:
 dd MULTIBOOT2_HEADER_MAGIC
 dd 0
 dd header_end - header_start
 dd 0x100000000 - (MULTIBOOT2_HEADER_MAGIC + 0 + header_end - header_start)

align 8
 dw 5   ; framebuffer request
 dw 0   ; flags
 dd 20  ; size
 dd 800 ; width
 dd 600 ; height
 dd 32  ; color scheme

align 8
 dw 0   ; end
 dw 0
 dd 8
header_end:


section .text
global _start
global _interrupt_array
global switch_context
global wc_memcpy

extern main
extern pic_eoi
extern interrupt_handler

_start:
 cli
 mov edx, eax
 call init_gdt

 mov esp, stack

 push edx
 push ebx
 call main

.j: hlt
 jmp .j





%macro MAKE_INTERRUPT_ENTRY 1
align 64
.i%1:
    pusha
    push %1
    call interrupt_handler
    add esp, 4
    popa
    iret

align 64
    times 16 dd 0
%endmacro


align 64
_interrupt_array:
%assign i 0
%rep 256
    MAKE_INTERRUPT_ENTRY i
%assign i i+1
%endrep

switch_context:
    mov eax, [esp+4]
    mov ecx, [esp+8]

    pusha
    mov [ecx], esp

    mov esp, eax
    popa

    ret


init_gdt:
 lgdt [gdt_descriptor]
 jmp 0x08:.reload_cs

.reload_cs:
 mov ax, 0x10
 mov ds, ax
 mov ss, ax
 mov es, ax
 mov fs, ax
 mov gs, ax
 ret
gdt:
 dd 0
 dd 0

 ;32bit code
 dw 0xFFFF ; end
 dw 0 ; base low 16bits
 db 0 ; base mid 8 bits
 db 0x9A ; rx
 db 0xCF ; flags
 db 0 ; base high 8 bits
 ;32bit data
 dw 0xFFFF
 dw 0
 db 0
 db 0x92; rw
 db 0xCF
 db 0

 ;16 bit code
 dw 0xFFFF
 dw 0
 db 0
 db 0x9A
 db 0
 db 0
 ;16bit data
 dw 0xFFFF
 dw 0
 db 0
 db 0x92
 db 0
 db 0
gdt_descriptor:
 dw gdt_descriptor-gdt -1
 dd gdt

section .bss
resb 65536
stack:
