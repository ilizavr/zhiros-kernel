#Copyright (c) 2026 ilizavr & yellowhat
#SPDX-License-Identifier: MIT

#clear
rm -rf build/ > /dev/null
mkdir build > /dev/null
mkdir ramdisk > /dev/null

set -e

FLAGS="-m32 -Wint-conversion -nostdlib -fno-pie -no-pie -fno-stack-protector -ffreestanding -O1 -w"
MODULE_FLAGS="$FLAGS -fPIC -fno-common"
ELF_MOD_FLAGS="$FLAGS -T ld/elf.ld"

build_module() {
    nasm ld/module_header.asm -f elf32 -o build/module_header.o
    gcc $MODULE_FLAGS $3 -c "$1" -o build/module.o
    ld -m elf_i386 -T ld/module.ld build/module.o build/module_header.o -o build/module.elf
    objcopy --set-section-flags .bss=alloc,load,contents -O binary build/module.elf "$2"
}

build_module_icon() {
    nasm ld/icon.asm -f elf32 -o build/icon.o
    gcc $MODULE_FLAGS $3 -c "$1" -o build/module.o
    ld -m elf_i386 -T ld/module.ld build/module.o build/icon.o -o build/module.elf
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
gcc $FLAGS -c kernel32/multitask.c -o build/multitask.o
nasm -f elf32 kernel32/kernel.asm -o build/kernel_asm.o
ld -m elf_i386 -T kernel32/linker.ld -o iso/boot/kernel.bin build/kernel_asm.o build/kernel_c.o build/printf.o build/allocator.o build/fbcon.o build/disk.o build/panic.o build/interrupt.o build/linker.o build/keyboard.o build/vfs.o build/shell.o build/timer.o build/multitask.o


#compile modules
build_module mod.clitools/clitools.c ramdisk/.clitools.mod
build_module mod.clitools/cli_NS.c ramdisk/.cli_NS.mod # NEUROSLOP WARNING
build_module mod.drivers/mouse.c ramdisk/.mouse.mod
build_module mod.drivers/bios_disk_driver.c ramdisk/.bios_disk_driver.mod
build_module mod.drivers/mbr_scan.c ramdisk/.mbr.mod
build_module mod.drivers/tmpfs.c ramdisk/.tmpfs.mod
build_module mod.win/window_manager.c ramdisk/.win.mod
build_module mod.win/macwm.c ramdisk/.mac.mod # NEUROSLOP WARNING
build_module_icon mod.win/terminal.c ramdisk/terminal.mod
build_module_icon mod.win/pong_wm.c ramdisk/pong.mod
build_module mod.win/img_view.c ramdisk/.img_view.mod
build_module_icon mod.win/doomloader.c ramdisk/doom.mod
build_module_icon mod.win/ZhirTE.c ramdisk/editor.mod
build_module mod.win/manager_program.c ramdisk/.mp.mod
build_module mod.win/zhirte_NS.c ramdisk/editor_NS.mod # NEUROSLOP WARNING
#bios disk driver 16bit blob
nasm mod.drivers/bios.asm -o ramdisk/bios.bin

#ZHIR VM
rm build/*.o
nasm ld/module_header.asm -f elf32 -o build/module_header.o
for f in mod.zvm/*.c; do gcc $MODULE_FLAGS -c "$f" -o build/$(basename "${f%.c}.o"); done
ld -m elf_i386 -T ld/module.ld build/*.o -o build/module.elf
objcopy --set-section-flags .bss=alloc,load,contents -O binary build/module.elf ramdisk/.zvm.mod

#create ramdisk image
tar --format=ustar -cf iso/boot/initrd.img -C ramdisk/ .

#create img
grub-mkrescue -o test.img iso/
