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

extern main
extern pic_eoi
extern interrupt_handler

_start:
 cli

 mov esp, stack

 push eax
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

section .bss
resb 65536
stack:
