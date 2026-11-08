ENTRY(init)

SECTIONS
{
    . = 0x0;

    .text : {
        *(.text.init)
        *(.text*)
    }

    _text_end = .;
    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }

    _end = .;
}
