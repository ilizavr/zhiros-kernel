/*  ide_disk_driver.c (v2) — IDE (ATA) диски для Zhiros, порты через PCI.
 *
 *  Что изменено после теста на реальном железе (в QEMU работало, на ПК висло):
 *   1. IRQ14/15 замаскированы в PIC + в Device Control выставлен nIEN=1 —
 *      контроллер вообще не выдаёт прерывания, драйвер чисто polling.
 *      Главная причина зависания на железе: INTRQ от контроллера при
 *      непонятном состоянии IRQ14/15 в ядре (в QEMU это не проявлялось).
 *   2. Весь порт-ввод/вывод — собственный inline-asm (ide_inb/outb/inw/outw),
 *      чтобы не зависеть от реализации inw/outw в ports.h.
 *   3. IDENTIFY по стандартной для железа последовательности: после команды
 *      проверяются 0x00/0xFF (пустая шина) и сигнатура ATAPI (0x14/0xEB),
 *      ide_poll выходит сразу при ERR — без долгого ожидания DRQ.
 *   4. Опрашиваются ОБА канала: primary (BAR0/BAR1 или 0x1F0/0x3F6) и
 *      secondary (BAR2/BAR3 или 0x170/0x376); повторный опрос канала
 *      исключён (ide_channel_seen).
 *   5. Диагностика: для каждого PCI mass storage печатается bus:slot:func,
 *      subclass и progif; AHCI (sub 6, progif 1) выявляется и пропускается.
 *   6. Для native-mode BAR включается декодирование I/O в PCI command.
 *   7. FLUSH CACHE после записи: результат игнорируется (на старых дисках
 *      команда даёт ERR, но данные уже записаны).
 */

#include "../lib/zhirtypes.h"
#include "../lib/string.h"
#include "../lib/ports.h"
#include "../lib/zhirapi.h"

#define IDE_MAX_DISKS   8

static bool mbrscan = false;    /* автоматически сканировать MBR после diskadd */
static u32  ide_found = 0;      /* найдено дисков */

/* ================================================================== */
/*  Порт-ввод/вывод: собственный inline-asm — не зависит от ports.h   */
/* ================================================================== */

static inline void ide_outb(u16 port, u8 val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 ide_inb(u16 port)
{
    u8 val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void ide_outw(u16 port, u16 val)
{
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline u16 ide_inw(u16 port)
{
    u16 val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void ide_outd(u16 port, u32 val)
{
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline u32 ide_ind(u16 port)
{
    u32 val;
    __asm__ volatile("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* polling-драйвер: прерывания IDE не нужны — глушим насовсем */
static void ide_irq_mask(void)
{
    ide_outb(0x21, ide_inb(0x21) | 0x40);   /* PIC1: маскируем IRQ14 */
    ide_outb(0xA1, ide_inb(0xA1) | 0x80);   /* PIC2: маскируем IRQ15 */
}

/* ================================================================== */
/*  PCI: чтение конфигурационного пространства (из pci.h)             */
/* ================================================================== */

static u16 pci_read_word(u8 bus, u8 slot, u8 func, u8 offset)
{
    u32 address = (u32)((bus << 16) | (slot << 11) | (func << 8) |
    (offset & 0xFC) | 0x80000000);
    ide_outd(0xCF8, address);
    return (u16)((ide_ind(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
}

static u32 pci_read_dword(u8 bus, u8 slot, u8 func, u8 offset)
{
    u32 address = (u32)((bus << 16) | (slot << 11) | (func << 8) |
    (offset & 0xFC) | 0x80000000);
    ide_outd(0xCF8, address);
    return ide_ind(0xCFC);
}

/* включить декодирование портов I/O в PCI command (нужно для native BAR) */
static void pci_enable_io(u8 bus, u8 slot, u8 func)
{
    u32 addr = (u32)((bus << 16) | (slot << 11) | (func << 8) | 0x80000000);
    ide_outd(0xCF8, addr | 0x04);
    u32 cmd = ide_ind(0xCFC);
    if (!(cmd & 0x1))
    {
        ide_outd(0xCF8, addr | 0x04);
        ide_outd(0xCFC, cmd | 0x1);
    }
}

/* ================================================================== */
/*  ATA-логика (из neuro_ide.h, доработана под реальное железо)       */
/* ================================================================== */

struct ide_device
{
    u16 base;       /* командные порты канала:  BAR0   (compat: 0x1F0/0x170) */
    u16 control;    /* порт управления каналом: BAR1+2 (compat: 0x3F6/0x376) */
    u8  drive;      /* 0 = master (0xA0/0xE0), 1 = slave (0xB0/0xF0)         */
};

static struct ide_device ide_drives[IDE_MAX_DISKS];
static char ide_names[IDE_MAX_DISKS][41];   /* строки моделей для disk->name */

static u16 probed_base[16];
static u16 probed_control[16];
static u32 probed_count = 0;

/* канал уже опрашивали? — защита от двойной регистрации */
static bool ide_channel_seen(u16 base, u16 control)
{
    for (u32 i = 0; i < probed_count; i++)
        if (probed_base[i] == base && probed_control[i] == control) return true;
        if (probed_count < 16)
        {
            probed_base[probed_count]    = base;
            probed_control[probed_count] = control;
            probed_count++;
        }
        return false;
}

/* пауза ~400 нс — 4 чтения из порта управления (alt status) */
static void ide_delay(u16 control)
{
    ide_inb(control);
    ide_inb(control);
    ide_inb(control);
    ide_inb(control);
}

/* ждать BSY=0 (и DRQ=1, если check_drq). ERR/таймаут -> 0 */
static u8 ide_poll(u16 base, bool check_drq)
{
    for (u32 i = 0; i < 1000000; i++)
    {
        u8 status = ide_inb(base + 7);
        if (status & 0x01) return 0;                    /* ERR — выходим сразу */
            if (!(status & 0x80))                           /* BSY=0 */
                if (!check_drq || (status & 0x08)) return 1;
    }
    return 0;
}

/* мягкий сброс канала: SRST=1 -> SRST=0,nIEN=1 -> ждать BSY=0 */
static void ide_reset_channel(u16 control)
{
    ide_outb(control, 0x04);            /* SRST=1 */
    ide_delay(control);
    ide_outb(control, 0x02);            /* SRST=0, nIEN=1 */
    ide_delay(control);

    for (u32 i = 0; i < 2000000; i++)   /* до ~2 сек */
    {
        u8 status = ide_inb(control);   /* alternate status */
        if (status == 0xFF) break;      /* пустая шина */
            if (!(status & 0x80)) break;    /* BSY=0 */
    }
}

/* IDENTIFY DEVICE: 1 = диск найден, info[] заполнен */
static bool ide_identify(u16 base, u16 control, u8 drive, u16 info[256])
{
    ide_outb(control, 0x02);            /* nIEN=1: без IRQ14/15 */
    ide_outb(base + 6, drive == 0 ? 0xA0 : 0xB0);
    ide_delay(control);

    ide_outb(base + 2, 0);
    ide_outb(base + 3, 0);
    ide_outb(base + 4, 0);
    ide_outb(base + 5, 0);
    ide_outb(base + 7, 0xEC);           /* IDENTIFY DEVICE */
    ide_delay(control);

    u8 status = ide_inb(base + 7);
    if (status == 0 || status == 0xFF) return false;    /* устройства нет */

        /* сигнатура ATAPI (CD/DVD): LBA mid/high = 0x14/0xEB */
        if (ide_inb(base + 4) == 0x14 && ide_inb(base + 5) == 0xEB) return false;

        if (!ide_poll(base, true)) return false;            /* BSY=0 + DRQ=1 (или ERR) */

            for (int i = 0; i < 256; i++)
                info[i] = ide_inw(base);

    return true;
}

/* чтение секторов LBA28, по одному сектору за команду */
static bool ide_read_sectors(struct ide_device* dev, u32 lba, char* buffer, u32 blocks)
{
    u16 base = dev->base;
    u16* ptr = (u16*)buffer;

    for (u32 i = 0; i < blocks; i++)
    {
        u32 cur = lba + i;

        ide_outb(base + 6, (dev->drive == 0 ? 0xE0 : 0xF0) | ((cur >> 24) & 0x0F));
        ide_outb(base + 1, 0);                  /* features */
        ide_outb(base + 2, 1);                  /* count    */
        ide_outb(base + 3, cur & 0xFF);         /* LBA low  */
        ide_outb(base + 4, (cur >> 8) & 0xFF);  /* LBA mid  */
        ide_outb(base + 5, (cur >> 16) & 0xFF); /* LBA high */
        ide_outb(base + 7, 0x20);               /* READ SECTORS */

        ide_delay(dev->control);

        if (!ide_poll(base, true)) return false;

        for (int j = 0; j < 256; j++)           /* 256 слов = 512 байт */
            *ptr++ = ide_inw(base);
    }
    return true;
}

/* запись секторов LBA28 + FLUSH CACHE после каждого сектора */
static bool ide_write_sectors(struct ide_device* dev, u32 lba, char* buffer, u32 blocks)
{
    u16 base = dev->base;
    u16* ptr = (u16*)buffer;

    for (u32 i = 0; i < blocks; i++)
    {
        u32 cur = lba + i;

        ide_outb(base + 6, (dev->drive == 0 ? 0xE0 : 0xF0) | ((cur >> 24) & 0x0F));
        ide_outb(base + 1, 0);
        ide_outb(base + 2, 1);
        ide_outb(base + 3, cur & 0xFF);
        ide_outb(base + 4, (cur >> 8) & 0xFF);
        ide_outb(base + 5, (cur >> 16) & 0xFF);
        ide_outb(base + 7, 0x30);               /* WRITE SECTORS */

        ide_delay(dev->control);

        if (!ide_poll(base, true)) return false;

        for (int j = 0; j < 256; j++)
            ide_outw(base, *ptr++);

        ide_outb(base + 7, 0xE7);               /* FLUSH CACHE */
        ide_delay(dev->control);
        ide_poll(base, false);                  /* на старых дисках ERR — данные уже записаны */
    }
    return true;
}

/* колбэки struct disk — тот же прототип, что bios_read_lba */
static bool ide_read_lba(struct disk* dsk, u32 lba, char* buffer, u32 blocks)
{
    return ide_read_sectors((struct ide_device*)dsk->other_info, lba, buffer, blocks);
}

static bool ide_write_lba(struct disk* dsk, u32 lba, char* buffer, u32 blocks)
{
    return ide_write_sectors((struct ide_device*)dsk->other_info, lba, buffer, blocks);
}

/* ================================================================== */
/*  Опрос канала и регистрация дисков                                 */
/* ================================================================== */

static void ide_add_disk(u16 base, u16 control)
{
    if (ide_channel_seen(base, control)) return;

    ide_reset_channel(control);

    for (u8 drive = 0; drive < 2; drive++)  /* master и slave */
    {
        u16 info[256];
        if (!ide_identify(base, control, drive, info)) continue;

        u32 sectors = *((u32*)(info + 60)); /* words 60-61: число секторов LBA28 */
        if (sectors == 0) continue;

        /* модель: 20 слов, байты в словах переставлены местами */
        char model[41];
        for (int i = 0; i < 20; i++)
        {
            u16 w = info[27 + i];
            model[i * 2]     = (char)(w >> 8);
            model[i * 2 + 1] = (char)(w & 0xFF);
        }
        model[40] = 0;

        int len = 39;
        while (len >= 0 && (model[len] == ' ' || model[len] == 0))
        {
            model[len] = 0;
            len--;
        }
        if (model[0] == 0) memcpy(model, "ide", 4);

        /* свободный слот */
        int idx = -1;
        for (int i = 0; i < IDE_MAX_DISKS; i++)
            if (!ide_drives[i].base) { idx = i; break; }
            if (idx == -1) return;

            ide_drives[idx].base    = base;
        ide_drives[idx].control = control;
        ide_drives[idx].drive   = drive;

        memcpy(ide_names[idx], model, 41);

        NEW(struct disk, n);
        n->name       = ide_names[idx];     /* имя модели диска */
        n->other_info = &ide_drives[idx];   /* как в bios: данные драйвера */
        n->size       = sectors;
        n->lba_read   = ide_read_lba;
        n->lba_write  = ide_write_lba;      /* если в struct disk нет lba_write — убрать */

        if (mbrscan) APICALL("_mbrscan", n);
        diskadd(n);
        ide_found++;

        LOGI("ide disk connected: slot %u, %u sectors", (u32)idx, sectors);
    }
}

/* ================================================================== */
/*  Определение портов через PCI (из pci.h)                           */
/* ================================================================== */

/* mass storage (класс 0x01): порты primary/secondary из BAR или legacy */
static void pci_check_disk(u8 bus, u8 slot, u8 func)
{
    u16 vendor_id = pci_read_word(bus, slot, func, 0x00);
    if (vendor_id == 0xFFFF) return;

    u16 class_word = pci_read_word(bus, slot, func, 0x0A);
    u8 subclass = class_word & 0xFF;
    u8 class    = (class_word >> 8) & 0xFF;
    if (class != 1) return;             /* не mass storage */

        u8 progif = pci_read_word(bus, slot, func, 0x08) & 0xFF;

    /* диагностика: по этой строке видно, что за контроллер */
    LOGI("ide: pci %u:%u:%u sub %u progif %u", bus, slot, func, subclass, progif);

    /* AHCI: subclass 6, progif 1 — legacy-порты там не декодируются */
    if (subclass == 6 && (progif & 0x0F) == 1)
    {
        LOGW("ide: AHCI not supported. switch SATA to IDE/compat mode in BIOS");
        return;
    }

    u32 bar0 = pci_read_dword(bus, slot, func, 0x10);
    u32 bar1 = pci_read_dword(bus, slot, func, 0x14);
    u32 bar2 = pci_read_dword(bus, slot, func, 0x18);
    u32 bar3 = pci_read_dword(bus, slot, func, 0x1C);

    u16 pbase, pcontrol, sbase, scontrol;

    if ((bar0 & 0x1) && (bar1 & 0x1))   /* native mode: порты из BAR */
    {
        pci_enable_io(bus, slot, func);
        pbase    = (u16)(bar0 & 0xFFFC);
        pcontrol = (u16)(bar1 & 0xFFFC) + 2;
        sbase    = ((bar2 & 0x1) && (bar3 & 0x1)) ? (u16)(bar2 & 0xFFFC) : 0x170;
        scontrol = ((bar2 & 0x1) && (bar3 & 0x1)) ? (u16)(bar3 & 0xFFFC) + 2 : 0x376;
    }
    else                                /* compat mode / пустые BAR: legacy */
    {
        LOGW("ide: MMIO/empty BARs. try to use legacy ports");
        pbase    = 0x1F0;
        pcontrol = 0x3F6;
        sbase    = 0x170;
        scontrol = 0x376;
    }

    LOGI("ide: primary base %u control %u", pbase, pcontrol);
    ide_add_disk(pbase, pcontrol);

    LOGI("ide: secondary base %u control %u", sbase, scontrol);
    ide_add_disk(sbase, scontrol);
}

static void pci_scan_ide(void)
{
    for (u16 bus = 0; bus < 256; bus++)
    {
        for (u8 slot = 0; slot < 32; slot++)
        {
            u16 vendor_id = pci_read_word(bus, slot, 0, 0x00);
            if (vendor_id == 0xFFFF) continue;

            pci_check_disk(bus, slot, 0);

            u16 header_type = pci_read_word(bus, slot, 0, 0x0E);
            if (header_type & 0x80)     /* мультфункциональное устройство */
            {
                for (u8 func = 1; func < 8; func++)
                    pci_check_disk(bus, slot, func);
            }
        }
    }
}

/* ================================================================== */
/*  Точка входа — по образцу bios_disk_driver.c                       */
/* ================================================================== */

INIT void init(void* (*_resolve_function)(char* name))
{
    api_init(_resolve_function);

    ide_irq_mask();                     /* polling: IRQ14/15 не нужны */

    LOGW("ide disk driver is unstable. continue loading?(y/n)");
    char key;
    while (key = getchar())
    {
        if (key == 'y') break;
        if (key == 'n') return;
    }

    LOGW("use auto mbrscan?(y/n)");
    while (key = getchar())
    {
        if (key == 'y') { mbrscan = true; break; }
        if (key == 'n') break;
    }

    pci_scan_ide();

    /* PCI ничего не нашёл — разрешаем ручной опрос legacy-портов */
    if (!ide_found)
    {
        LOGW("ide: nothing found via pci. probe legacy ports?(y/n)");
        while (key = getchar())
        {
            if (key == 'y')
            {
                ide_add_disk(0x1F0, 0x3F6);
                ide_add_disk(0x170, 0x376);
                break;
            }
            if (key == 'n') break;
        }
    }

    LOGI("ide disk driver inited. found %u disk", ide_found);
}
