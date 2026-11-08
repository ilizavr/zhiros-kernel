CC      := gcc
LD      := ld
OBJCOPY := objcopy

BUILDDIR := ../build
MODDIR   := ../ramdisk

CFLAGS  := -m32 -fno-stack-protector -ffreestanding -O0 -fPIC -fno-common -w
LDFLAGS := -m elf_i386 -T ../ld/module.ld

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,$(BUILDDIR)/%.o,$(SRCS))

ELF := $(BUILDDIR)/module.elf
MOD := $(MODDIR)/zvm.mod

.PHONY: all module kernel run test clean

all: kernel run

module: $(MOD)

kernel: module
	cd .. && sh build32.sh

run:
	cd .. && qemu-system-x86_64 -hda test.img -m 3064 -full-screen

test:
	./tests/run_tests.sh

$(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(MOD): $(ELF)
	$(OBJCOPY) --set-section-flags .bss=alloc,load,contents -O binary $< $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -f $(OBJS) $(ELF)
