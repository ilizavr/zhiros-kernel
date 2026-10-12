#include "../lib/zhirtypes.h"
#include "../lib/string.h"
#include "../lib/ports.h"
#include "../lib/zhirapi.h"
#include "pci.h"
#include "e1000.h"

static void lspci_cmd(void)
{
    pci_scan();
}

static void netinfo_cmd(void)
{
    e1000_print_info();
}

static void nettx_cmd(void)
{
    int result = e1000_send_test_frame();
    if (result == 0) printf("E1000 TX: complete\n");
    else printf("E1000 TX: error %d\n", result);
}

static void netrx_cmd(void)
{
    u32 length = 0;
    int result = e1000_receive_frame(e1000_test_rx_frame,
                                     sizeof(e1000_test_rx_frame), &length);
    if (result == 1) printf("E1000 RX: no frame\n");
    else if (result < 0) printf("E1000 RX: error %d\n", result);
    else printf("E1000 RX: frame length %u\n", length);
}

INIT void init(void* (*_resolve_function)(char* name))
{
    api_init(_resolve_function);

    register_function("lspci", lspci_cmd, "print all pci device info");
    register_function("netinfo", netinfo_cmd, "print network device info");
    register_function("nettx", nettx_cmd, "send test Ethernet frame");
    register_function("netrx", netrx_cmd, "poll Ethernet RX ring");

    register_function("_pci_read_word", pci_read_word, "_pci_read_word(u8 bus, u8 slot, u8 func, u8 offset) -> u16");
    register_function("_pci_read_dword", pci_read_dword, "_pci_read_dword(u8 bus, u8 slot, u8 func, u8 offset) -> u32");
    register_function("_pci_write_word", pci_write_word, "_pci_write_word(u8 bus, u8 slot, u8 func, u8 offset, u16 val) -> None");
    register_function("_pci_find_device", pci_find_device, "_pci_find_device(u16 vendor, u16 device, u8 *b, u8 *s, u8 *f) -> int");

    register_function("_e1000_send", e1000_send_frame, "_e1000_send(const void *frame, u32 length) -> int");
    register_function("_e1000_recv", e1000_receive_frame, "_e1000_recv(void *frame, u32 capacity, u32 *length) -> int");
    register_function("_e1000_get_mac", e1000_get_mac, "_e1000_get_mac(u8 *mac) -> int");
    register_function("_e1000_probe", e1000_probe, "_e1000_probe() -> int");

    if (e1000_probe()) {
        LOGW("E1000 probe failed");
    } else {
        LOGI("E1000 initialized");
        e1000_print_info();
    }
}
