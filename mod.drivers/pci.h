#ifndef ZHIROS_PCI_H
#define ZHIROS_PCI_H

static u16 pci_read_word(u8 bus, u8 slot, u8 func, u8 offset)
{
    u32 address = (u32)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000u);
    port_dword_out(0xCF8, address);
    return (u16)((port_dword_in(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
}

static u32 pci_read_dword(u8 bus, u8 slot, u8 func, u8 offset)
{
    u32 address = (u32)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000u);
    port_dword_out(0xCF8, address);
    return port_dword_in(0xCFC);
}

static void pci_write_word(u8 bus, u8 slot, u8 func, u8 offset, u16 value)
{
    u32 address = (u32)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000u);
    port_dword_out(0xCF8, address);
    port_word_out(0xCFC + (offset & 2), value);
}

static int pci_find_device(u16 wanted_vendor, u16 wanted_device,
                           u8 *bus_out, u8 *slot_out, u8 *func_out)
{
    for (u32 bus = 0; bus < 256; bus++) {
        for (u8 slot = 0; slot < 32; slot++) {
            for (u8 func = 0; func < 8; func++) {
                u16 vendor = pci_read_word((u8)bus, slot, func, 0x00);
                if (vendor == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }
                u16 device = pci_read_word((u8)bus, slot, func, 0x02);
                if (vendor != wanted_vendor || device != wanted_device)
                    continue;
                if (bus_out) *bus_out = (u8)bus;
                if (slot_out) *slot_out = slot;
                if (func_out) *func_out = func;
                return 0;
            }
        }
    }
    return -1;
}

static void pci_check_device(u8 bus, u8 slot, u8 func)
{
    u16 vendor_id = pci_read_word(bus, slot, func, 0x00);
    if (vendor_id == 0xFFFF) return;

    u16 device_id = pci_read_word(bus, slot, func, 0x02);
    u16 class_word = pci_read_word(bus, slot, func, 0x0A);
    u8 subclass = class_word & 0xFF;
    u8 class_code = (class_word >> 8) & 0xFF;

    printf("PCI [B:%u D:%u F:%u] Vendor:0x%x Device:0x%x Class:0x%x Sub:0x%x\n",
           (u32)bus, (u32)slot, (u32)func,
           (u32)vendor_id, (u32)device_id,
           (u32)class_code, (u32)subclass);
}

static void pci_scan(void)
{
    for (u32 bus = 0; bus < 256; bus++) {
        for (u8 slot = 0; slot < 32; slot++) {
            u16 vendor_id = pci_read_word((u8)bus, slot, 0, 0x00);
            if (vendor_id == 0xFFFF) continue;

            pci_check_device((u8)bus, slot, 0);

            u16 header_type = pci_read_word((u8)bus, slot, 0, 0x0E);
            if (header_type & 0x80) {
                for (u8 func = 1; func < 8; func++) {
                    pci_check_device((u8)bus, slot, func);
                }
            }
        }
    }
}

#endif
