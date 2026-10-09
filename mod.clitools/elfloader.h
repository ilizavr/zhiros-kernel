/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */

#define EI_NIDENT 16
#define ELF_MAGIC 0x464C457FU
#define PT_LOAD   1

struct elf32_hdr {
    u8  e_ident[EI_NIDENT];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u32 e_entry;
    u32 e_phoff;
    u32 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
} __attribute__((packed));

struct elf32_phdr {
    u32 p_type;
    u32 p_offset;
    u32 p_vaddr;
    u32 p_paddr;
    u32 p_filesz;
    u32 p_memsz;
    u32 p_flags;
    u32 p_align;
} __attribute__((packed));

static bool elf_load(char diskletter, char *name)
{
    struct file *f = open(diskletter, name);
    if (!f) {
        LOGE("file not found");
        return false;
    }

    int size = f->getsize(f);
    if (size >= 0x10000) {
        LOGE("file too big");
        f->close(f);
        return false;
    }

    u8 *file_buf = (u8 *)alloc(size);
    f->read(f, file_buf, size, 0);
    f->close(f);

    struct elf32_hdr *header = (struct elf32_hdr *)file_buf;

    if (*(u32 *)header->e_ident != ELF_MAGIC) {
        LOGE("Not a valid ELF file");
        free(file_buf);
        return false;
    }

    struct elf32_phdr *phdr = (struct elf32_phdr *)(file_buf + header->e_phoff);
    for (u16 i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            if((phdr[i].p_vaddr+phdr[i].p_filesz)>0x100000)
            {
                LOGE("ELF file have invalid load address");
                free(file_buf);
                return false;
            }
            void *dest = (void *)phdr[i].p_vaddr;
            void *src  = file_buf + phdr[i].p_offset;

            memcpy(dest, src, phdr[i].p_filesz);

            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset((u8 *)dest + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    CALL(header->e_entry,resolve_function);
    return true;
}
