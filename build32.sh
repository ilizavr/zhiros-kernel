#Copyright (c) 2026 ilizavr & yellowhat
#SPDX-License-Identifier: MIT

#clear
rm -rf build/
mkdir build
mkdir ramdisk

FLAGS="-m32 -Wint-conversion -nostdlib -fno-pie -no-pie -fno-stack-protector -ffreestanding -O0 -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-div-by-zero"
MODULE_FLAGS="$FLAGS -fPIC -fno-common"
ELF_MOD_FLAGS="$FLAGS -T ld/elf.ld"

build_module() {
    gcc $MODULE_FLAGS -c "$1" -o build/module.o
    ld -m elf_i386 -T ld/module.ld build/module.o -o build/module.elf
    objcopy --set-section-flags .bss=alloc,load,contents -O binary build/module.elf "$2"
}


#compile kernel
gcc $FLAGS -c kernel32/kernel.c -o build/kernel_c.o
gcc $FLAGS -c kernel32/allocator.c -o build/allocator.o
gcc $FLAGS -c kernel32/shell/fbcon.c -o build/fbcon.o
gcc $FLAGS -c kernel32/printf.c -o build/printf.o
gcc $FLAGS -c kernel32/disk.c -o build/disk.o
gcc $FLAGS -c kernel32/panic.c -o build/panic.o
gcc $FLAGS -c kernel32/interrupt.c -o build/interrupt.o
gcc $FLAGS -c kernel32/linker.c -o build/linker.o
gcc $FLAGS -c kernel32/vfs.c -o build/vfs.o
gcc $FLAGS -c kernel32/shell/keyboard.c -o build/keyboard.o
gcc $FLAGS -c kernel32/shell/shell.c -o build/shell.o
gcc $FLAGS -c kernel32/timer.c -o build/timer.o
nasm -f elf32 kernel32/kernel.asm -o build/kernel_asm.o
ld -m elf_i386 -T kernel32/linker.ld -o iso/boot/kernel.bin build/kernel_asm.o build/kernel_c.o build/printf.o build/allocator.o build/fbcon.o build/disk.o build/panic.o build/interrupt.o build/linker.o build/keyboard.o build/vfs.o build/shell.o build/timer.o


#compile modules
build_module mod.clitools/clitools.c ramdisk/clitools.mod
build_module mod.drivers/mouse.c ramdisk/mouse.mod
build_module mod.games/pong.c ramdisk/pong.mod
build_module mod.examples/hello_mod.c ramdisk/hello.mod
gcc $ELF_MOD_FLAGS mod.examples/hello_elf.c -o ramdisk/hello.elf

#create ramdisk image
tar --format=ustar -cf iso/boot/initrd.img -C ramdisk/ .

#create iso
grub-mkrescue -o test.img iso/
