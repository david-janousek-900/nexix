#include "net/kernel.h"
#include "net/if.h"
#include "net/arp.h"
#include "net/route.h"
#include "net/ip.h"
#include "net/icmp.h"
#include "net/udp.h"
#include "net/tcp.h"
#include "net/compat.h"
#include "net/e1000.h"
#include "net/worker.h"
#include "net/wire.h"
#include "console.h"
#include "memory.h"
#include "process.h"
#define REQUIRE(c) do { if (!(c)) panic("network stack self-test: " #c); } while (0)

/* Temporary sinks: a local-experimental ethertype counts loopback frames, and
 * IP protocol 253 (RFC 3692 experimental) counts datagrams; both check that
 * byte i of the payload is seed + i. */
#define TEST_ETHERTYPE 0x88b5u
#define TEST_PROTOCOL 253u
static volatile unsigned link_packets, ip_packets;
static volatile size_t ip_length;
static struct nx_net_if *link_ifp, *ip_ifp;
static void check_pattern(const struct nx_net_mbuf *m, size_t offset, size_t length)
{
    uint8_t bytes[64], seed = 0;
    for (size_t done = 0; done < length; done += sizeof(bytes)) {
        size_t part = length - done < sizeof(bytes) ? length - done : sizeof(bytes);
        REQUIRE(nx_net_mbuf_copydata(m, offset + done, part, bytes));
        if (!done) seed = bytes[0];
        for (size_t i = 0; i < part; ++i) REQUIRE(bytes[i] == (uint8_t)(seed + done + i));
    }
}
static void link_sink(struct nx_net_if *ifp, struct nx_net_mbuf *m)
{
    REQUIRE(net_worker_context() && (m->flags & NX_NET_M_PKTHDR));
    size_t length = nx_net_mbuf_length(m);
    REQUIRE(length >= 2 && length <= 64);
    check_pattern(m, 0, length);
    nx_net_mbuf_freem(m);
    link_ifp = ifp;
    ++link_packets;
}
static void ip_sink(struct nx_net_if *ifp, struct nx_net_mbuf *m, size_t header_length)
{
    REQUIRE(net_worker_context() && header_length == 20 && m->len >= 20 && m->data[9] == TEST_PROTOCOL);
    size_t length = nx_net_mbuf_length(m);
    REQUIRE(nx_net_get16(m->data + 2) == length && nx_net_get16(m->data + 6) == 0);
    check_pattern(m, header_length, length - header_length);
    nx_net_mbuf_freem(m);
    ip_ifp = ifp;
    ip_length = length - header_length;
    ++ip_packets;
}
static struct nx_net_mbuf *packet(size_t length, uint8_t seed)
{
    uint8_t bytes[64];
    struct nx_net_mbuf *m = nx_net_mbuf_get(net_pool(), true);
    REQUIRE(m);
    for (size_t done = 0; done < length; done += sizeof(bytes)) {
        size_t part = length - done < sizeof(bytes) ? length - done : sizeof(bytes);
        for (size_t i = 0; i < part; ++i) bytes[i] = (uint8_t)(seed + done + i);
        REQUIRE(nx_net_mbuf_append(m, bytes, part));
    }
    return m;
}

static unsigned accepted;
static void register_sinks(void *a)
{
    (void)a;
    REQUIRE(nx_net_proto_register(TEST_ETHERTYPE, link_sink) == NX_NET_OK);
    REQUIRE(nx_net_proto_register(NX_NET_ETHERTYPE_ARP, link_sink) == NX_NET_EXISTS);   /* the stack's */
    REQUIRE(nx_net_proto_register(NX_NET_ETHERTYPE_IP, link_sink) == NX_NET_EXISTS);
    REQUIRE(nx_net_ip_register(TEST_PROTOCOL, ip_sink) == NX_NET_OK);
}
static void unregister_sinks(void *a)
{
    (void)a;
    nx_net_proto_unregister(TEST_ETHERTYPE);
    nx_net_ip_unregister(TEST_PROTOCOL);
}
/* Queue more than lo0 holds within one worker pass: nothing is delivered
 * until this call returns, and the overflow is dropped and counted. */
static void loop_burst(void *a)
{
    (void)a;
    struct nx_net_if *lo = nx_net_if_find("lo0");
    accepted = 0;
    for (unsigned i = 0; i < NX_NET_IFQ_MAXLEN + 5; ++i)
        accepted += nx_net_if_output(lo, packet(20 + i % 40, (uint8_t)i), NULL, TEST_ETHERTYPE) == NX_NET_OK;
    REQUIRE(link_packets == 0 && lo->queue_length == NX_NET_IFQ_MAXLEN);
}
static bool em_ok;
static void em_up(void *a) { (void)a; em_ok = nx_net_if_up(nx_net_if_find("em0")) == NX_NET_OK; }
static void em_down(void *a) { (void)a; nx_net_if_down(nx_net_if_find("em0")); }
static void em_states(void *a)
{
    (void)a;
    struct nx_net_if *em = nx_net_if_find("em0");
    /* Down: refused; oversize: refused; both free their chains. */
    REQUIRE(nx_net_if_output(em, packet(20, 0), nx_net_ether_broadcast, NX_NET_ETHERTYPE_IP) == NX_NET_DOWN);
    struct nx_net_mbuf *big = nx_net_mbuf_get(net_pool(), true);
    static uint8_t zero[NX_NET_ETHER_MTU + 1];
    REQUIRE(big && nx_net_mbuf_append(big, zero, sizeof(zero)));
    REQUIRE(nx_net_if_up(em) == NX_NET_OK);
    REQUIRE(em->lladdr[0] == 0x52 && em->lladdr[1] == 0x54 && (em->flags & NX_NET_IFF_RUNNING));
    REQUIRE(nx_net_if_output(em, big, nx_net_ether_broadcast, NX_NET_ETHERTYPE_IP) == NX_NET_TOO_BIG);
    REQUIRE(em->stats.oerrors == 2);
}
#define IP(a, b, c, d) ((nx_net_addr_t)(a) << 24 | (nx_net_addr_t)(b) << 16 | (nx_net_addr_t)(c) << 8 | (d))
/* QEMU user networking: 10.0.2.0/24, gateway 10.0.2.2 (52:55:0a:00:02:02),
 * which answers ARP only for its own service addresses. */
static enum nx_net_result routed;
static nx_net_addr_t route_target;
static void configure(void *a)
{
    (void)a;
    struct nx_net_if *lo = nx_net_if_find("lo0"), *em = nx_net_if_find("em0");
    REQUIRE(nx_net_route_set_default(IP(10, 0, 2, 2)) == NX_NET_UNREACHABLE);   /* nothing on-link yet */
    REQUIRE(nx_net_inet_set(lo, IP(127, 0, 0, 1), IP(255, 0, 0, 0)) == NX_NET_OK);
    REQUIRE(nx_net_inet_set(em, IP(10, 0, 2, 0), IP(255, 255, 255, 0)) == NX_NET_INVALID);
    REQUIRE(nx_net_inet_set(em, IP(10, 0, 2, 15), IP(255, 255, 255, 0)) == NX_NET_OK);
    REQUIRE(nx_net_route_set_default(IP(10, 0, 2, 2)) == NX_NET_OK);
    struct nx_net_route r;
    REQUIRE(nx_net_route_lookup(IP(192, 0, 2, 1), &r) == NX_NET_OK && r.ifp == em && r.gateway &&
            r.next_hop == IP(10, 0, 2, 2));
    REQUIRE(nx_net_route_lookup(IP(10, 0, 2, 15), &r) == NX_NET_OK && r.ifp == lo && r.local);
}
static size_t send_length = 32;
static bool send_df;
static void route_packet(void *a)
{
    struct nx_net_ip_send send = { .dst = route_target, .protocol = TEST_PROTOCOL, .dont_fragment = send_df };
    routed = nx_net_ip_output(packet(send_length, (uint8_t)(uintptr_t)a), &send);
}
static struct nx_net_ip_stats ip;
static void loop_address(void *a)
{
    (void)a;
    REQUIRE(nx_net_inet_set(nx_net_if_find("lo0"), IP(127, 0, 0, 1), IP(255, 0, 0, 0)) == NX_NET_OK);
}
static void ip_probe(void *a) { (void)a; ip = nx_net_ip_stats(); }
/* Datagrams over lo0: small, fragmented above its 16384-byte MTU and
 * reassembled, and refused with don't-fragment. */
static void loop_ip(struct nx_net_if *lo)
{
    REQUIRE(net_worker_call(loop_address, NULL));
    route_target = IP(127, 0, 0, 1);
    static const size_t lengths[] = {0, 32, 20000};
    for (unsigned i = 0; i < 3; ++i) {
        ip_packets = 0;
        send_length = lengths[i];
        REQUIRE(net_worker_call(route_packet, (void *)(uintptr_t)(3 + i)) && routed == NX_NET_OK);
        for (unsigned t = 0; t < 100 && !ip_packets; ++t) proc_delay(1);
        REQUIRE(ip_packets == 1 && ip_ifp == lo && ip_length == lengths[i]);
    }
    send_df = true;
    REQUIRE(net_worker_call(route_packet, (void *)1) && routed == NX_NET_TOO_BIG);
    send_df = false;
    send_length = 32;
    REQUIRE(net_worker_call(ip_probe, NULL) && ip.fragmented == 1 && ip.fragments_sent == 2 &&
            ip.reassembled == 1 && ip.cant_fragment == 1 && nx_net_ip_reassembling() == 0);
    REQUIRE(net_pool()->stats.mbufs == 0 && net_pool()->stats.clusters == 0);
}
/* A raw ICMP socket: echo requests answered by our own stack or QEMU's
 * gateway, and protocol unreachable for an unregistered protocol. */
#define PING_ID 0x4e58u
static struct nx_net_raw *raw;
static enum nx_net_result raw_result;
static uint8_t received[128];
static size_t received_length;
static void raw_open(void *a)
{
    (void)a;
    struct nx_net_raw *denied = NULL;
    REQUIRE(nx_net_raw_open(false, NULL, NULL, &denied) == NX_NET_DENIED && !denied);
    REQUIRE(nx_net_raw_open(true, NULL, NULL, &raw) == NX_NET_OK && nx_net_raw_open_count() == 1);
}
static void raw_close(void *a) { (void)a; nx_net_raw_close(raw); }
static void ping(void *a)
{
    uint8_t message[8 + 32] = { NX_NET_ICMP_ECHO };
    nx_net_put16(message + 4, PING_ID);
    nx_net_put16(message + 6, (uint16_t)(uintptr_t)a);
    for (unsigned i = 8; i < sizeof(message); ++i) message[i] = (uint8_t)i;
    struct nx_net_mbuf *m = nx_net_mbuf_get(net_pool(), true);
    REQUIRE(m && nx_net_mbuf_append(m, message, sizeof(message)));
    raw_result = nx_net_raw_send(raw, m, route_target);
}
static void raw_take(void *a)
{
    (void)a;
    struct nx_net_mbuf *m = nx_net_raw_receive(raw);
    received_length = 0;
    if (!m) return;
    size_t length = nx_net_mbuf_length(m);
    received_length = length < sizeof(received) ? length : sizeof(received);
    REQUIRE(nx_net_mbuf_copydata(m, 0, received_length, received));
    nx_net_mbuf_freem(m);
}
static uint16_t checksum(const uint8_t *bytes, size_t length)
{
    struct nx_net_checksum sum;
    nx_net_checksum_init(&sum);
    nx_net_checksum_update(&sum, bytes, length);
    return nx_net_checksum_finish(&sum);
}
/* Wait up to ticks for a datagram of type from source; checksums must hold. */
static bool await_icmp(uint8_t type, nx_net_addr_t source, unsigned ticks)
{
    for (unsigned t = 0; t < ticks; ++t) {
        REQUIRE(net_worker_call(raw_take, NULL));
        if (!received_length) { proc_delay(1); continue; }
        size_t hlen = (size_t)(received[0] & 0xf) * 4;
        REQUIRE(received_length >= hlen + 8 && received_length < sizeof(received));
        REQUIRE(!checksum(received, hlen) && !checksum(received + hlen, received_length - hlen));
        if (received[hlen] == type && nx_net_get32(received + 12) == source) return true;
    }
    return false;
}
static bool echoed(uint16_t sequence, nx_net_addr_t source, unsigned ticks)
{
    route_target = source;
    REQUIRE(net_worker_call(ping, (void *)(uintptr_t)sequence) && raw_result == NX_NET_OK);
    while (await_icmp(NX_NET_ICMP_ECHOREPLY, source, ticks))
        if (nx_net_get16(received + 24) == PING_ID && nx_net_get16(received + 26) == sequence &&
            received_length == 20 + 40 && received[28] == 8 && received[59] == 39) return true;
    return false;
}
static void unknown_packet(void *a)
{
    (void)a;
    struct nx_net_ip_send send = { .dst = IP(127, 0, 0, 1), .protocol = 254 };
    routed = nx_net_ip_output(packet(16, 1), &send);
}
static struct nx_net_icmp_stats icmp;
static void icmp_probe(void *a) { (void)a; icmp = nx_net_icmp_stats(); }
static void loop_icmp(void)
{
    REQUIRE(net_worker_call(raw_open, NULL));
    REQUIRE(echoed(1, IP(127, 0, 0, 1), 100));
    /* Protocol 254 has no handler: an unreachable error quotes the datagram. */
    route_target = IP(127, 0, 0, 1);
    REQUIRE(net_worker_call(ip_probe, NULL));
    uint64_t unknown = ip.no_protocol;
    REQUIRE(net_worker_call(unknown_packet, NULL) && routed == NX_NET_OK);
    REQUIRE(await_icmp(NX_NET_ICMP_UNREACH, IP(127, 0, 0, 1), 100));
    REQUIRE(received[21] == NX_NET_ICMP_UNREACH_PROTOCOL && received[28 + 9] == 254 && received_length == 20 + 8 + 20 + 8);
    REQUIRE(net_worker_call(ip_probe, NULL) && ip.no_protocol == unknown + 1);
    REQUIRE(net_worker_call(icmp_probe, NULL) && icmp.echoes >= 1 && icmp.replies_sent >= 1 && icmp.errors_sent >= 1);
    REQUIRE(net_worker_call(raw_close, NULL));
    REQUIRE(net_pool()->stats.mbufs == 0 && net_pool()->stats.clusters == 0);
}
/* UDP over lo0: a 9216-byte datagram (fragmented and reassembled), a reply
 * to the sender's ephemeral port, and port unreachable for a closed port. */
static struct nx_net_udp *udp_server, *udp_client;
static enum nx_net_result udp_result;
static size_t udp_length;
static nx_net_addr_t udp_from;
static nx_net_port_t udp_port;
static void udp_open(void *a)
{
    (void)a;
    REQUIRE(nx_net_udp_open(NULL, NULL, &udp_server) == NX_NET_OK && nx_net_udp_open(NULL, NULL, &udp_client) == NX_NET_OK);
    REQUIRE(nx_net_udp_bind(udp_server, IP(127, 0, 0, 1), 7, false) == NX_NET_DENIED);
    REQUIRE(nx_net_udp_bind(udp_server, IP(127, 0, 0, 1), 7, true) == NX_NET_OK);
    REQUIRE(nx_net_udp_connect(udp_client, IP(127, 0, 0, 1), 7) == NX_NET_OK &&
            udp_client->local_port >= NX_NET_UDP_EPHEMERAL_FIRST);
}
static void udp_send(void *a)
{
    struct nx_net_udp *u = a == udp_server ? udp_server : udp_client;
    udp_result = nx_net_udp_send(u, packet(u == udp_server ? 3 : NX_NET_UDP_SNDMAX, 5),
                                 u == udp_server ? udp_from : 0, u == udp_server ? udp_port : 0);
}
static void udp_take(void *a)
{
    struct nx_net_mbuf *m = nx_net_udp_receive(a, &udp_from, &udp_port);
    udp_length = SIZE_MAX;
    if (!m) return;
    udp_length = nx_net_mbuf_length(m);
    check_pattern(m, 0, udp_length);
    nx_net_mbuf_freem(m);
}
static void udp_close_server(void *a) { (void)a; nx_net_udp_close(udp_server); }
static void udp_error(void *a) { (void)a; udp_result = nx_net_udp_take_error(udp_client); }
static void udp_close_client(void *a) { (void)a; nx_net_udp_close(udp_client); }
static void await_udp(struct nx_net_udp *u)
{
    for (unsigned t = 0; t < 100; ++t) {
        REQUIRE(net_worker_call(udp_take, u));
        if (udp_length != SIZE_MAX) return;
        proc_delay(1);
    }
}
static void loop_udp(void)
{
    REQUIRE(net_worker_call(udp_open, NULL));
    REQUIRE(net_worker_call(udp_send, udp_client) && udp_result == NX_NET_OK);
    await_udp(udp_server);
    REQUIRE(udp_length == NX_NET_UDP_SNDMAX && udp_from == IP(127, 0, 0, 1) && udp_port == udp_client->local_port);
    REQUIRE(net_worker_call(udp_send, udp_server) && udp_result == NX_NET_OK);
    await_udp(udp_client);
    REQUIRE(udp_length == 3 && udp_port == 7);
    REQUIRE(net_worker_call(udp_close_server, NULL));
    REQUIRE(net_worker_call(udp_send, udp_client) && udp_result == NX_NET_OK);
    udp_result = NX_NET_OK;
    for (unsigned t = 0; t < 100 && udp_result == NX_NET_OK; ++t) {
        proc_delay(1);
        REQUIRE(net_worker_call(udp_error, NULL));
    }
    REQUIRE(udp_result == NX_NET_REFUSED);
    REQUIRE(net_worker_call(udp_close_client, NULL));
    REQUIRE(net_pool()->stats.mbufs == 0 && net_pool()->stats.clusters == 0);
}
/* TCP over lo0: 64 KiB sent through a listener's accepted connection and
 * echoed back, then closed from both sides. */
#define TCP_BYTES 65536u
static struct {
    struct nx_net_tcp *listener, *client, *server;
    size_t written, received, echoed, returned;
    bool client_shut, server_shut, done;
    uint8_t got[TCP_BYTES];
} tcp;
static uint8_t tcp_byte(size_t i) { return (uint8_t)(i * 13 + 1); }
static void tcp_open(void *a)
{
    (void)a;
    REQUIRE(nx_net_tcp_open(NULL, NULL, &tcp.listener) == NX_NET_OK);
    REQUIRE(nx_net_tcp_bind(tcp.listener, IP(127, 0, 0, 1), 7, false) == NX_NET_DENIED);
    REQUIRE(nx_net_tcp_bind(tcp.listener, IP(127, 0, 0, 1), 7, true) == NX_NET_OK && nx_net_tcp_listen(tcp.listener, 1) == NX_NET_OK);
    REQUIRE(nx_net_tcp_open(NULL, NULL, &tcp.client) == NX_NET_OK && nx_net_tcp_connect(tcp.client, IP(127, 0, 0, 1), 7) == NX_NET_OK);
}
static void tcp_step(void *a)
{
    (void)a;
    uint8_t chunk[512];
    size_t n;
    if (!tcp.server && nx_net_tcp_accept(tcp.listener, NULL, NULL, &tcp.server) == NX_NET_OK) {
        nx_net_tcp_close(tcp.listener);
        tcp.listener = NULL;
    }
    while (tcp.client->state == NX_NET_TCP_ESTABLISHED && tcp.written < TCP_BYTES) {
        size_t part = TCP_BYTES - tcp.written < sizeof(chunk) ? TCP_BYTES - tcp.written : sizeof(chunk);
        for (size_t i = 0; i < part; ++i) chunk[i] = tcp_byte(tcp.written + i);
        if (nx_net_tcp_write(tcp.client, chunk, part, &n) != NX_NET_OK) break;
        tcp.written += n;
    }
    if (tcp.written == TCP_BYTES && !tcp.client_shut) tcp.client_shut = nx_net_tcp_shutdown(tcp.client) == NX_NET_OK;
    if (tcp.server) {
        while (tcp.received < TCP_BYTES &&
               nx_net_tcp_read(tcp.server, tcp.got + tcp.received, TCP_BYTES - tcp.received, &n) == NX_NET_OK && n)
            tcp.received += n;
        while (tcp.echoed < tcp.received &&
               nx_net_tcp_write(tcp.server, tcp.got + tcp.echoed, tcp.received - tcp.echoed, &n) == NX_NET_OK && n)
            tcp.echoed += n;
        if (tcp.received == TCP_BYTES && tcp.echoed == TCP_BYTES && !tcp.server_shut &&
            nx_net_tcp_read(tcp.server, chunk, 1, &n) == NX_NET_OK && !n)
            tcp.server_shut = nx_net_tcp_shutdown(tcp.server) == NX_NET_OK;
    }
    while (nx_net_tcp_read(tcp.client, chunk, sizeof(chunk), &n) == NX_NET_OK && n) {
        for (size_t i = 0; i < n; ++i) REQUIRE(chunk[i] == tcp_byte(tcp.returned + i));
        tcp.returned += n;
    }
    tcp.done = tcp.returned == TCP_BYTES && tcp.server && tcp.server->state == NX_NET_TCP_CLOSED &&
               tcp.client->state == NX_NET_TCP_TIME_WAIT;
}
static void tcp_close(void *a)
{
    (void)a;
    REQUIRE(nx_net_tcp_take_error(tcp.client) == NX_NET_OK && nx_net_tcp_take_error(tcp.server) == NX_NET_OK);
    nx_net_tcp_close(tcp.server);
    nx_net_tcp_close(tcp.client);   /* stays in TIME_WAIT, holding no buffers */
    REQUIRE(nx_net_tcp_used() == 1);
}
static void loop_tcp(void)
{
    tcp.listener = tcp.client = tcp.server = NULL;
    tcp.written = tcp.received = tcp.echoed = tcp.returned = 0;
    tcp.client_shut = tcp.server_shut = tcp.done = false;
    REQUIRE(net_worker_call(tcp_open, NULL));
    for (unsigned t = 0; t < 1000 && !tcp.done; ++t) {
        REQUIRE(net_worker_call(tcp_step, NULL));
        if (!tcp.done) proc_delay(1);
    }
    REQUIRE(tcp.done);
    for (size_t i = 0; i < TCP_BYTES; ++i) REQUIRE(tcp.got[i] == tcp_byte(i));
    REQUIRE(net_worker_call(tcp_close, NULL));
    REQUIRE(net_pool()->stats.mbufs == 0 && net_pool()->stats.clusters == 0);
}
static bool resolved;
static uint8_t resolved_mac[NX_NET_ETHER_ADDR];
static struct nx_net_arp_stats arp;
static void arp_probe(void *a)
{
    arp = nx_net_arp_stats();
    resolved = nx_net_arp_lookup(nx_net_if_find("em0"), (nx_net_addr_t)(uintptr_t)a, resolved_mac);
}
static void routes(struct nx_net_if *lo, struct nx_net_if *em, bool slow)
{
    REQUIRE(net_worker_call(configure, NULL));
    /* Our own em0 address is delivered through lo0. */
    ip_packets = 0;
    route_target = IP(10, 0, 2, 15);
    REQUIRE(net_worker_call(route_packet, (void *)7) && routed == NX_NET_OK);
    for (unsigned t = 0; t < 100 && !ip_packets; ++t) proc_delay(1);
    REQUIRE(ip_packets == 1 && ip_ifp == lo);
    /* An off-link destination resolves the gateway; the held packet follows. */
    uint64_t sent = em->stats.opackets;
    route_target = IP(192, 0, 2, 1);
    REQUIRE(net_worker_call(route_packet, (void *)9) && routed == NX_NET_OK);
    resolved = false;
    for (unsigned t = 0; t < 600 && !resolved; ++t) {
        proc_delay(1);
        REQUIRE(net_worker_call(arp_probe, (void *)(uintptr_t)IP(10, 0, 2, 2)));
    }
    REQUIRE(resolved && resolved_mac[0] == 0x52 && resolved_mac[1] == 0x55 && resolved_mac[5] == 0x02);
    REQUIRE(em->stats.opackets >= sent + 2 && arp.received >= 1 && arp.requests >= 1);
    /* QEMU's gateway answers echo requests over em0. */
    REQUIRE(net_worker_call(raw_open, NULL));
    REQUIRE(echoed(2, IP(10, 0, 2, 2), 200));
    REQUIRE(net_worker_call(raw_close, NULL));
    if (!slow) return;
    /* An absent on-link peer is retried five times, then refused. */
    route_target = IP(10, 0, 2, 77);
    REQUIRE(net_worker_call(route_packet, (void *)11) && routed == NX_NET_OK);
    uint64_t timeouts = arp.timeouts;
    for (unsigned t = 0; t < 800 && arp.timeouts == timeouts; ++t) {
        proc_delay(1);
        REQUIRE(net_worker_call(arp_probe, (void *)(uintptr_t)IP(10, 0, 2, 77)));
    }
    REQUIRE(arp.timeouts == timeouts + 1 && arp.hold_drops >= 1 && !resolved);
    REQUIRE(net_worker_call(route_packet, (void *)12) && routed == NX_NET_HOST_UNREACHABLE);
}

static void tests(void *argument)
{
    (void)argument;
    int32_t status;
    uint32_t pages = page_free_count();
    for (unsigned round = 0; round < 2; ++round) {
        REQUIRE(net_start() && !net_start() && net_pool());
        REQUIRE(net_worker_call(register_sinks, NULL));
        struct nx_net_if *lo = nx_net_if_find("lo0"), *em = nx_net_if_find("em0");
        REQUIRE(lo && (lo->flags & (NX_NET_IFF_UP | NX_NET_IFF_RUNNING | NX_NET_IFF_LOOPBACK)) ==
                (NX_NET_IFF_UP | NX_NET_IFF_RUNNING | NX_NET_IFF_LOOPBACK));
        REQUIRE(nx_net_if_count() == (em ? 2u : 1u) && (!em || !(em->flags & NX_NET_IFF_UP)));

        link_packets = 0;
        REQUIRE(net_worker_call(loop_burst, NULL) && accepted == NX_NET_IFQ_MAXLEN);
        for (unsigned t = 0; t < 100 && link_packets < NX_NET_IFQ_MAXLEN; ++t) proc_delay(1);
        REQUIRE(link_packets == NX_NET_IFQ_MAXLEN && link_ifp == lo && lo->queue_length == 0);
        REQUIRE(lo->stats.iqdrops == 5 && lo->stats.opackets == NX_NET_IFQ_MAXLEN);
        REQUIRE(net_pool()->stats.mbufs == 0);   /* every looped packet was freed once */
        if (!round) console_puts("NET LOOPBACK OK\r\n");
        loop_ip(lo);
        if (!round) console_puts("NET IP OK\r\n");
        loop_icmp();
        if (!round) console_puts("NET ICMP OK\r\n");
        loop_udp();
        if (!round) console_puts("NET UDP OK\r\n");
        loop_tcp();
        if (!round) console_puts("NET TCP OK\r\n");

        if (em) {
            REQUIRE(net_worker_call(em_states, NULL));
            /* QEMU holds receive for ~1 s after enabling it; ARP retries cover that. */
            routes(lo, em, !round);
            REQUIRE(em->stats.ipackets >= 1);
            if (!round) console_puts("NET ROUTE OK\r\n");
            REQUIRE(net_worker_call(em_down, NULL) && !e1000_started() && !(em->flags & NX_NET_IFF_UP));
            REQUIRE(net_worker_call(arp_probe, (void *)(uintptr_t)IP(10, 0, 2, 2)) && !resolved);   /* flushed */
            REQUIRE(net_worker_call(em_up, NULL) && em_ok && e1000_started());   /* restartable */
            if (!round) console_puts("NET ETHERNET OK\r\n");
        } else if (!round) {
            console_puts("NET ETHERNET ABSENT\r\n");
        }
        REQUIRE(net_worker_call(unregister_sinks, NULL));
        net_stop();
        REQUIRE(!net_pool() && nx_net_if_count() == 0 && !e1000_started());
        REQUIRE(proc_wait(&status) > 0 && status == 0);   /* reap the worker */
        REQUIRE(page_free_count() == pages);
    }
    proc_exit(0);
}

void net_stack_selftest(void)
{
    uint32_t before = page_free_count();
    v7_pid_t pid = proc_spawn(tests, NULL);
    REQUIRE(pid > 0);
    proc_run();
    int32_t status;
    REQUIRE(proc_collect(&status) == pid && status == 0);
    REQUIRE(page_free_count() == before);
    console_puts("NET STACK OK\r\n");
}
