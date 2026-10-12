#include "../lib/zhirtypes.h"
#include "../lib/string.h"
#include "../lib/zhirapi.h"
#include "ethernet.h"
#include "arp.h"
#include "ipv4.h"
#include "tcp.h"
#include "udp.h"
#include "dhcp.h"

static void net_hexdump(const u8 *data, u32 size)
{
    const char *hex_digits = "0123456789ABCDEF";
    char line[80];
    for (u32 i = 0; i < size; i += 16) {
        u32 chunk = size - i;
        if (chunk > 16) chunk = 16;
        u32 pos = 0;
        for (int j = 28; j >= 0; j -= 4)
            line[pos++] = hex_digits[(i >> j) & 0x0F];
        line[pos++] = ':';
        line[pos++] = ' ';
        for (u32 j = 0; j < 16; j++) {
            if (j < chunk) {
                u8 b = data[i + j];
                line[pos++] = hex_digits[b >> 4];
                line[pos++] = hex_digits[b & 0x0F];
            } else {
                line[pos++] = ' ';
                line[pos++] = ' ';
            }
            line[pos++] = ' ';
            if (j == 7) line[pos++] = ' ';
        }
        line[pos++] = ' ';
        line[pos++] = '|';
        for (u32 j = 0; j < chunk; j++) {
            u8 b = data[i + j];
            line[pos++] = (b >= 32 && b <= 126) ? (char)b : '.';
        }
        line[pos++] = '|';
        line[pos++] = '\n';
        line[pos] = 0;
        printf("%s", line);
    }
}

#define NET_ARP_WAIT_TICKS 100
#define NET_PING_RETRIES 3
#define NET_DHCP_WAIT_TICKS 200
#define NET_DHCP_RETRIES 3

static net_send_frame_function drv_send_frame;
static net_receive_frame_function drv_receive_frame;
static int (*drv_get_mac)(u8 *mac);

static struct net_arp_state net_arp_state;
static struct net_ipv4_state net_ipv4_state;
static struct net_tcp_state net_tcp_state;
static struct net_udp_state net_udp_state;
static struct net_dhcp_state net_dhcp_state;
static struct net_udp_datagram net_received_datagram;
static u8 net_tcp_receive_buffer[NET_TCP_RECV_BUFFER_SIZE];
static u8 net_local_ip[4];
static u8 net_gateway_ip[4];
static volatile u32 net_lock;

static int net_lock_acquire(void)
{
    u32 eflags = save_irq();
    if (net_lock) {
        restore_irq(eflags);
        return 0;
    }
    net_lock = 1;
    restore_irq(eflags);
    return 1;
}

static void net_lock_release(void)
{
    u32 eflags = save_irq();
    net_lock = 0;
    restore_irq(eflags);
}

static void net_step_unlocked(void)
{
    if (!drv_send_frame || !drv_receive_frame) return;
    u32 now = getticks();
    net_arp_tick(&net_arp_state);
    net_tcp_tick(&net_tcp_state, &net_arp_state, now, drv_send_frame);
    net_dhcp_tick(&net_dhcp_state, &net_arp_state,
                  &net_ipv4_state, &net_udp_state, now,
                  drv_send_frame);
    for (u32 index = 0; index < 8; index++) {
        int result = net_network_poll(
            &net_ipv4_state, &net_udp_state, &net_tcp_state,
            &net_arp_state, drv_receive_frame, drv_send_frame);
        if (result == NET_NETWORK_POLL_UDP_QUEUED)
            net_dhcp_poll(&net_dhcp_state, &net_arp_state,
                          &net_ipv4_state, &net_udp_state,
                          now, 1000, drv_send_frame);
        if (result <= 0) break;
    }
}

static void net_default_target(u8 *target)
{
    if (net_dhcp_state.bound &&
        (net_dhcp_state.gateway[0] | net_dhcp_state.gateway[1] |
         net_dhcp_state.gateway[2] | net_dhcp_state.gateway[3])) {
        memcpy(target, net_dhcp_state.gateway, 4);
    } else {
        memcpy(target, net_gateway_ip, 4);
    }
}

static int net_wait_for_arp(const u8 *target, u32 timeout)
{
    u32 start = getticks();
    while ((u32)(getticks() - start) < timeout) {
        if (net_lock_acquire()) {
            net_step_unlocked();
            u8 mac[NET_ETHERNET_ADDRESS_LENGTH];
            int result = net_arp_lookup(&net_arp_state, target, mac);
            net_lock_release();
            if (result == 0) return 0;
        }
        yield();
    }
    return -1;
}

static int net_wait_for_echo(u32 previous, u32 timeout)
{
    u32 start = getticks();
    while ((u32)(getticks() - start) < timeout) {
        if (net_lock_acquire()) {
            net_step_unlocked();
            u32 received = net_ipv4_state.echo_received;
            net_lock_release();
            if (received != previous) return 0;
        }
        yield();
    }
    return -1;
}

static int netarp_ip(char *ip_text)
{
    u8 target[4];
    if (!ip_text || !*ip_text) {
        net_default_target(target);
    } else if (net_parse_ipv4(ip_text, target)) {
        printf("use _netarp;ipv4\n");
        return -1;
    }
    if (!drv_send_frame || !net_lock_acquire()) {
        printf("network busy\n");
        return -1;
    }
    int result = net_arp_request(&net_arp_state, target, drv_send_frame);
    net_lock_release();
    printf("ARP request: %s\n", result == 0 ? "sent" : "error");
    return result;
}

static void netarp_cmd(void)
{
    netarp_ip(0);
}

static int netping_ip(char *ip_text)
{
    u8 target[4];
    if (!ip_text || !*ip_text) {
        net_default_target(target);
    } else if (net_parse_ipv4(ip_text, target)) {
        printf("use _netping;ipv4\n");
        return -1;
    }
    if (!drv_send_frame) {
        printf("network driver not ready\n");
        return -1;
    }
    int last_error = -1;
    for (u32 attempt = 0; attempt < NET_PING_RETRIES; attempt++) {
        if (attempt) {
            if (!net_lock_acquire()) {
                printf("network busy\n");
                return -1;
            }
            net_arp_invalidate(&net_arp_state, target);
            net_lock_release();
        }
        if (!net_lock_acquire()) {
            printf("network busy\n");
            return -1;
        }
        u32 previous = net_ipv4_state.echo_received;
        int result = net_ipv4_send_echo(&net_ipv4_state, &net_arp_state,
                                        target, drv_send_frame);
        net_lock_release();
        if (result == 1) {
            if (net_wait_for_arp(target, NET_ARP_WAIT_TICKS)) {
                last_error = -2;
                continue;
            }
            if (!net_lock_acquire()) {
                printf("network busy\n");
                return -1;
            }
            previous = net_ipv4_state.echo_received;
            result = net_ipv4_send_echo(&net_ipv4_state, &net_arp_state,
                                        target, drv_send_frame);
            net_lock_release();
        }
        if (result) {
            last_error = result;
            continue;
        }
        if (!net_wait_for_echo(previous, NET_ARP_WAIT_TICKS)) {
            printf("ICMP echo reply: received\n");
            return 0;
        }
        last_error = -3;
    }
    printf("ICMP echo: timeout/error %d\n", last_error);
    return last_error;
}

static void netping_cmd(void)
{
    netping_ip(0);
}

static int netudp_ip_port(char *ip_text, u32 port_arg)
{
    u8 target[4];
    if (!ip_text || !*ip_text) {
        net_default_target(target);
    } else if (net_parse_ipv4(ip_text, target)) {
        printf("use _netudp;ipv4;i:port\n");
        return -1;
    }
    u16 port = port_arg ? (u16)port_arg : NET_UDP_ECHO_PORT;
    if (!drv_send_frame || !net_lock_acquire()) {
        printf("network busy\n");
        return -1;
    }
    static const u8 payload[] = "zhiros udp";
    int result = net_udp_send(&net_udp_state, &net_arp_state,
                              target, port, payload, sizeof(payload) - 1,
                              drv_send_frame);
    net_lock_release();
    if (result == 1) {
        if (!net_wait_for_arp(target, NET_ARP_WAIT_TICKS)) {
            if (!net_lock_acquire()) {
                printf("network busy\n");
                return -1;
            }
            result = net_udp_send(&net_udp_state, &net_arp_state,
                                  target, port, payload, sizeof(payload) - 1,
                                  drv_send_frame);
            net_lock_release();
        } else {
            printf("UDP datagram: ARP timeout\n");
            return -2;
        }
    }
    if (result == 0) printf("UDP datagram: sent\n");
    else printf("UDP datagram: error %d\n", result);
    return result;
}

static void netudp_cmd(void)
{
    netudp_ip_port(0, NET_UDP_ECHO_PORT);
}

static void dhcp_cmd(void)
{
    if (!drv_send_frame) {
        printf("network driver not ready\n");
        return;
    }
    for (u32 attempt = 0; attempt < NET_DHCP_RETRIES; attempt++) {
        if (!net_lock_acquire()) {
            printf("network busy\n");
            return;
        }
        net_udp_unbind(&net_udp_state, NET_DHCP_CLIENT_PORT);
        net_dhcp_init(&net_dhcp_state, net_arp_state.local_mac,
                      0x5A49524F + attempt);
        net_arp_clear_cache(&net_arp_state);
        int result = net_dhcp_start(&net_dhcp_state, &net_arp_state,
                                    &net_udp_state, drv_send_frame);
        net_lock_release();
        if (result) {
            printf("DHCP discover error %d\n", result);
            return;
        }
        u32 start = getticks();
        while ((u32)(getticks() - start) < NET_DHCP_WAIT_TICKS) {
            if (!net_lock_acquire()) {
                yield();
                continue;
            }
            u32 now = getticks();
            net_step_unlocked();
            result = net_dhcp_poll(&net_dhcp_state, &net_arp_state,
                                   &net_ipv4_state, &net_udp_state,
                                   now, 1000, drv_send_frame);
            u32 dhcp_bound = net_dhcp_state.bound;
            net_lock_release();
            if (result == NET_DHCP_BOUND || dhcp_bound) {
                printf("DHCP bound %u.%u.%u.%u lease %u\n",
                       (u32)net_dhcp_state.offered_ip[0],
                       (u32)net_dhcp_state.offered_ip[1],
                       (u32)net_dhcp_state.offered_ip[2],
                       (u32)net_dhcp_state.offered_ip[3],
                       net_dhcp_state.lease_time);
                return;
            }
            if (result < 0) break;
            if (result == NET_DHCP_OFFER_RECEIVED) start = getticks();
            yield();
        }
        printf("DHCP retry\n");
    }
    printf("DHCP timeout\n");
}

static int udplisten_cmd(u32 port_arg)
{
    if (!port_arg || port_arg > 65535) {
        printf("use _udplisten;i:port\n");
        return -1;
    }
    if (!net_lock_acquire()) {
        printf("network busy\n");
        return -1;
    }
    int result = net_udp_bind(&net_udp_state, (u16)port_arg);
    net_lock_release();
    if (!result) printf("UDP listen: bound\n");
    else printf("UDP listen: error %d\n", result);
    return result;
}

static int udpunbind_cmd(u32 port_arg)
{
    if (!port_arg || port_arg > 65535) {
        printf("use _udpunbind;i:port\n");
        return -1;
    }
    if (!net_lock_acquire()) {
        printf("network busy\n");
        return -1;
    }
    int result = net_udp_unbind(&net_udp_state, (u16)port_arg);
    net_lock_release();
    if (!result) printf("UDP listen: unbound\n");
    else printf("UDP unbind: error %d\n", result);
    return result;
}

static int udprecv_cmd(u32 port_arg)
{
    if (!port_arg || port_arg > 65535) {
        printf("use _udprecv;i:port\n");
        return -1;
    }
    if (!net_lock_acquire()) {
        printf("network busy\n");
        return -1;
    }
    int result = net_udp_receive(&net_udp_state, (u16)port_arg,
                                 &net_received_datagram);
    net_lock_release();
    if (result == 1) {
        printf("UDP receive: no datagram\n");
        return 1;
    }
    if (result) {
        printf("UDP receive: error %d\n", result);
        return result;
    }
    printf("UDP from %u.%u.%u.%u:%u length %u\n",
           (u32)net_received_datagram.source_ip[0],
           (u32)net_received_datagram.source_ip[1],
           (u32)net_received_datagram.source_ip[2],
           (u32)net_received_datagram.source_ip[3],
           (u32)net_received_datagram.source_port,
           (u32)net_received_datagram.length);
    net_hexdump(net_received_datagram.payload, net_received_datagram.length);
    return 0;
}

static int udpsend_cmd(char *ip_text, u32 port_arg, char *text)
{
    u8 target[4];
    if (!ip_text || !text || !port_arg || port_arg > 65535 ||
        net_parse_ipv4(ip_text, target)) {
        printf("use _udpsend;ipv4;i:port;text\n");
        return -1;
    }
    u32 len = strlen(text);
    if (len > NET_UDP_PAYLOAD_MAX) {
        printf("use _udpsend;ipv4;i:port;text\n");
        return -1;
    }
    if (!drv_send_frame || !net_lock_acquire()) {
        printf("network busy\n");
        return -1;
    }
    int result = net_udp_send_from(&net_udp_state, &net_arp_state,
                                   net_udp_state.port, target, (u16)port_arg,
                                   text, (u16)len, drv_send_frame);
    net_lock_release();
    if (result == 1) {
        if (net_wait_for_arp(target, NET_ARP_WAIT_TICKS)) {
            printf("UDP send: ARP timeout\n");
            return -2;
        }
        if (!net_lock_acquire()) {
            printf("network busy\n");
            return -1;
        }
        result = net_udp_send_from(&net_udp_state, &net_arp_state,
                                   net_udp_state.port, target, (u16)port_arg,
                                   text, (u16)len, drv_send_frame);
        net_lock_release();
    }
    if (!result) printf("UDP send: sent\n");
    else printf("UDP send: error %d\n", result);
    return result;
}

static int tcplisten_cmd(u32 port_arg)
{
    if (!port_arg || port_arg > 65535) {
        printf("use _tcplisten;i:port\n");
        return -1;
    }
    if (!net_lock_acquire()) {
        printf("network busy\n");
        return -1;
    }
    int result = net_tcp_listen(&net_tcp_state, (u16)port_arg);
    net_lock_release();
    if (!result) printf("TCP listen: bound\n");
    else printf("TCP listen: error %d\n", result);
    return result;
}

static int tcpconnect_cmd(char *ip_text, u32 port_arg)
{
    u8 target[4];
    if (!ip_text || !port_arg || port_arg > 65535 ||
        net_parse_ipv4(ip_text, target)) {
        printf("use _tcpconnect;ipv4;i:port\n");
        return -1;
    }
    if (!drv_send_frame || !net_lock_acquire()) {
        printf("network busy\n");
        return -1;
    }
    int result = net_tcp_connect(&net_tcp_state, &net_arp_state,
                                 target, (u16)port_arg, drv_send_frame);
    net_lock_release();
    if (result == 1) {
        if (net_wait_for_arp(target, NET_ARP_WAIT_TICKS)) {
            printf("TCP connect: ARP timeout\n");
            return -2;
        }
        if (!net_lock_acquire()) {
            printf("network busy\n");
            return -1;
        }
        result = net_tcp_connect(&net_tcp_state, &net_arp_state,
                                 target, (u16)port_arg, drv_send_frame);
        net_lock_release();
    }
    if (!result) printf("TCP SYN: sent\n");
    else printf("TCP connect: error %d\n", result);
    return result;
}

static int tcpsend_cmd(char *text)
{
    if (!text) {
        printf("use _tcpsend;text\n");
        return -1;
    }
    u32 len = strlen(text);
    if (!len || len > NET_TCP_PAYLOAD_MAX) {
        printf("use _tcpsend;text\n");
        return -1;
    }
    if (!drv_send_frame || !net_lock_acquire()) {
        printf("network busy\n");
        return -1;
    }
    int result = net_tcp_send_data(&net_tcp_state, &net_arp_state,
                                   text, (u16)len, drv_send_frame);
    net_lock_release();
    if (!result) printf("TCP data: sent\n");
    else printf("TCP send: error %d\n", result);
    return result;
}

static int tcprecv_cmd(void)
{
    if (!net_lock_acquire()) {
        printf("network busy\n");
        return -1;
    }
    u16 length = 0;
    int result = net_tcp_receive_data(&net_tcp_state, net_tcp_receive_buffer,
                                      sizeof(net_tcp_receive_buffer), &length);
    net_lock_release();
    if (result == 1) {
        printf("TCP receive: no data\n");
        return 1;
    }
    if (result) {
        printf("TCP receive: error %d\n", result);
        return result;
    }
    printf("TCP data length %u\n", (u32)length);
    net_hexdump(net_tcp_receive_buffer, length);
    return 0;
}

static int tcpclose_cmd(void)
{
    if (!drv_send_frame || !net_lock_acquire()) {
        printf("network busy\n");
        return -1;
    }
    int result = net_tcp_close(&net_tcp_state, &net_arp_state,
                               drv_send_frame);
    net_lock_release();
    if (!result) printf("TCP FIN: sent\n");
    else printf("TCP close: error %d\n", result);
    return result;
}

static void tcpinfo_cmd(void)
{
    if (!net_lock_acquire()) {
        printf("network busy\n");
        return;
    }
    struct net_tcp_connection *connection =
        net_tcp_first_active(&net_tcp_state);
    u32 connection_used = connection ? connection->used : 0;
    u32 connection_state = connection ? connection->state : 0;
    u16 local_port = connection ? connection->local_port : 0;
    u16 remote_port = connection ? connection->remote_port : 0;
    u32 congestion_window = connection ? connection->congestion_window : 0;
    u32 slow_start_threshold = connection ? connection->slow_start_threshold : 0;
    u32 remote_window = connection ? connection->remote_window : 0;
    u16 remote_mss = connection ? connection->remote_mss : 0;
    u32 tx_count = connection ? connection->tx_count : 0;
    u32 reorder_count = connection ? connection->reorder_count : 0;
    u32 duplicate_ack_count = connection ? connection->duplicate_ack_count : 0;
    u32 retransmissions = connection ? connection->retransmissions : 0;
    u32 fast_retransmits = connection ? connection->fast_retransmits : 0;
    u32 sack_blocks = connection ? connection->sack_blocks : 0;
    u32 persist_probes = connection ? connection->persist_probes : 0;
    u32 established = net_tcp_state.established;
    u32 received = net_tcp_state.received;
    u32 sent = net_tcp_state.sent;
    u32 closed = net_tcp_state.closed;
    u32 malformed = net_tcp_state.malformed;
    u32 checksum_errors = net_tcp_state.checksum_errors;
    u32 option_errors = net_tcp_state.option_errors;
    u32 ack_errors = net_tcp_state.ack_errors;
    u32 total_retransmissions = net_tcp_state.retransmissions;
    u32 total_fast_retransmits = net_tcp_state.fast_retransmits;
    u32 total_sack_blocks = net_tcp_state.sack_blocks;
    u32 total_persist_probes = net_tcp_state.persist_probes;
    u32 persist_expired = net_tcp_state.persist_expired;
    net_lock_release();

    printf("TCP state:%u local:%u remote:%u cwnd:%u ssthresh:%u remote_window:%u remote_mss:%u txq:%u reorder:%u dupack:%u\n",
           connection_used ? connection_state : (u32)NET_TCP_STATE_CLOSED,
           (u32)local_port, (u32)remote_port,
           congestion_window, slow_start_threshold,
           remote_window, (u32)remote_mss,
           tx_count, reorder_count, duplicate_ack_count);
    printf("TCP counters tx:%u rx:%u established:%u closed:%u retransmit:%u fast:%u sack:%u persist:%u\n",
           sent, received, established, closed,
           retransmissions, fast_retransmits, sack_blocks, persist_probes);
    printf("TCP malformed:%u checksum:%u options:%u ack:%u total_retransmit:%u total_fast:%u total_sack:%u total_persist:%u persist_expired:%u\n",
           malformed, checksum_errors, option_errors, ack_errors,
           total_retransmissions, total_fast_retransmits,
           total_sack_blocks, total_persist_probes, persist_expired);
}

static void netpoll_cmd(void)
{
    if (!drv_send_frame || !drv_receive_frame || !net_lock_acquire()) {
        printf("network busy\n");
        return;
    }
    int result = net_network_poll(&net_ipv4_state, &net_udp_state,
                                  &net_tcp_state, &net_arp_state,
                                  drv_receive_frame, drv_send_frame);
    net_lock_release();
    if (result == 0) printf("network poll: no frame\n");
    else if (result == 1) printf("ICMP echo reply received\n");
    else if (result == 2) printf("ICMP echo request replied\n");
    else if (result == NET_IPV4_POLL_ARP_REPLY)
        printf("ARP reply received\n");
    else if (result == NET_IPV4_POLL_ARP_REQUEST_REPLIED)
        printf("ARP request replied\n");
    else if (result == NET_NETWORK_POLL_UDP_REPLIED)
        printf("UDP echo replied\n");
    else if (result == NET_NETWORK_POLL_UDP_QUEUED)
        printf("UDP datagram queued\n");
    else if (result == NET_NETWORK_POLL_UDP_DROPPED)
        printf("UDP datagram dropped\n");
    else if (result == NET_NETWORK_POLL_TCP_ESTABLISHED)
        printf("TCP connection established\n");
    else if (result == NET_NETWORK_POLL_TCP_DATA)
        printf("TCP data received\n");
    else if (result == NET_NETWORK_POLL_TCP_CLOSED)
        printf("TCP connection closed\n");
    else if (result == NET_NETWORK_POLL_TCP_FIN)
        printf("TCP FIN received\n");
    else printf("network poll: error %d\n", result);
}

static void netstats_cmd(void)
{
    if (!net_lock_acquire()) {
        printf("network busy\n");
        return;
    }
    u32 arp_requests = net_arp_state.requests_sent;
    u32 arp_received = net_arp_state.replies_received;
    u32 arp_sent = net_arp_state.replies_sent;
    u32 arp_hits = net_arp_state.cache_hits;
    u32 arp_misses = net_arp_state.cache_misses;
    u32 arp_expired = net_arp_state.cache_expired;
    u32 icmp_sent = net_ipv4_state.echo_sent;
    u32 icmp_received = net_ipv4_state.echo_received;
    u32 icmp_replied = net_ipv4_state.echo_replied;
    u32 udp_sent = net_udp_state.sent;
    u32 udp_received = net_udp_state.received;
    u32 udp_replied = net_udp_state.replied;
    u32 udp_queued = net_udp_state.queued;
    u32 udp_drops = net_udp_state.drops;
    u32 dhcp_state = net_dhcp_state.state;
    u32 dhcp_retries = net_dhcp_state.retries;
    u32 dhcp_bound = net_dhcp_state.bound;
    u32 tcp_sent = net_tcp_state.sent;
    u32 tcp_received = net_tcp_state.received;
    u32 tcp_established = net_tcp_state.established;
    u32 tcp_closed = net_tcp_state.closed;
    u32 tcp_drops = net_tcp_state.drops;
    net_lock_release();

    printf("ARP req:%u rx:%u tx:%u hit:%u miss:%u expired:%u\n",
           arp_requests, arp_received, arp_sent,
           arp_hits, arp_misses, arp_expired);
    printf("ICMP tx:%u rx:%u reply:%u | UDP tx:%u rx:%u reply:%u queued:%u drops:%u\n",
           icmp_sent, icmp_received, icmp_replied,
           udp_sent, udp_received, udp_replied, udp_queued, udp_drops);
    printf("DHCP state:%u retries:%u bound:%u | TCP tx:%u rx:%u established:%u closed:%u drops:%u\n",
           dhcp_state, dhcp_retries, dhcp_bound,
           tcp_sent, tcp_received, tcp_established, tcp_closed, tcp_drops);
}

static void net_process(void)
{
    while (true) {
        if (net_lock_acquire()) {
            net_step_unlocked();
            net_lock_release();
        }
        yield();
    }
}

INIT void init(void* (*_resolve_function)(char* name))
{
    api_init(_resolve_function);

    net_local_ip[0] = 10;
    net_local_ip[1] = 0;
    net_local_ip[2] = 2;
    net_local_ip[3] = 15;

    net_gateway_ip[0] = 10;
    net_gateway_ip[1] = 0;
    net_gateway_ip[2] = 2;
    net_gateway_ip[3] = 2;

    drv_send_frame = _resolve_function("_e1000_send");
    drv_receive_frame = _resolve_function("_e1000_recv");
    drv_get_mac = _resolve_function("_e1000_get_mac");

    if (!drv_send_frame || !drv_receive_frame || !drv_get_mac) {
        load_mod('A', "./e1000.mod");
        drv_send_frame = _resolve_function("_e1000_send");
        drv_receive_frame = _resolve_function("_e1000_recv");
        drv_get_mac = _resolve_function("_e1000_get_mac");
    }

    u8 mac[NET_ETHERNET_ADDRESS_LENGTH] = {0, 0, 0, 0, 0, 0};
    if (drv_get_mac) drv_get_mac(mac);

    net_arp_init(&net_arp_state, mac, net_local_ip);
    net_ipv4_init(&net_ipv4_state, net_local_ip);
    net_tcp_init(&net_tcp_state);
    net_udp_init(&net_udp_state, NET_UDP_ECHO_PORT);
    net_dhcp_init(&net_dhcp_state, mac, 0x5A49524F);

    register_function("netarp", netarp_cmd, "send ARP request to gateway");
    register_function("netping", netping_cmd, "send ICMP echo to gateway");
    register_function("netudp", netudp_cmd, "send UDP datagram to gateway");
    register_function("dhcp", dhcp_cmd, "request IPv4 lease");
    register_function("tcprecv", tcprecv_cmd, "receive TCP data");
    register_function("tcpclose", tcpclose_cmd, "close TCP connection");
    register_function("tcpinfo", tcpinfo_cmd, "print TCP state");
    register_function("netpoll", netpoll_cmd, "poll network frame");
    register_function("netstats", netstats_cmd, "print network counters");

    register_function("_netarp", netarp_ip, "_netarp;ipv4 -> send ARP request");
    register_function("_netping", netping_ip, "_netping;ipv4 -> send ICMP echo");
    register_function("_netudp", netudp_ip_port, "_netudp;ipv4;i:port -> send UDP datagram");
    register_function("_udplisten", udplisten_cmd, "_udplisten;i:port -> bind UDP receive port");
    register_function("_udpunbind", udpunbind_cmd, "_udpunbind;i:port -> unbind UDP receive port");
    register_function("_udprecv", udprecv_cmd, "_udprecv;i:port -> receive queued UDP datagram");
    register_function("_udpsend", udpsend_cmd, "_udpsend;ipv4;i:port;text -> send UDP text datagram");
    register_function("_tcplisten", tcplisten_cmd, "_tcplisten;i:port -> bind TCP listen port");
    register_function("_tcpconnect", tcpconnect_cmd, "_tcpconnect;ipv4;i:port -> connect TCP endpoint");
    register_function("_tcpsend", tcpsend_cmd, "_tcpsend;text -> send TCP text");
    register_function("_tcprecv", tcprecv_cmd, "_tcprecv -> receive TCP data");
    register_function("_tcpclose", tcpclose_cmd, "_tcpclose -> close TCP connection");

    create_task("network", net_process);
    LOGI("net module inited");
}
