#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net/arp.h"
#include "net/compat.h"
#include "net/tcp.h"
#include "net/wire.h"

static size_t live_pages;
static void *page_alloc(void *c) { (void)c; ++live_pages; void *p = aligned_alloc(4096, 4096); assert(p); return p; }
static void page_free(void *c, void *p) { (void)c; --live_pages; free(p); }
static const struct nx_net_page_ops ops = { page_alloc, page_free, NULL };
static struct nx_net_budget budget;
static struct nx_net_mbuf_pool pool;
static uint64_t now = 1000;
static uint64_t clock_ms(void) { return now; }

#define IP(a, b, c, d) ((nx_net_addr_t)(a) << 24 | (nx_net_addr_t)(b) << 16 | (nx_net_addr_t)(c) << 8 | (d))
#define ME IP(10, 0, 2, 15)
#define PEER IP(10, 0, 2, 2)
#define LOCALHOST IP(127, 0, 0, 1)
#define FIN 0x01
#define SYN 0x02
#define RST 0x04
#define PSH 0x08
#define ACK 0x10

/* Fake Ethernet driver: records IPv4 packets (without the link header). */
#define FRAMES 64
static uint8_t frames[FRAMES][1600];
static size_t frame_lengths[FRAMES], sent;
static bool fake_transmit(struct nx_net_if *i, struct nx_net_mbuf *m)
{
    (void)i;
    size_t length = nx_net_mbuf_length(m);
    uint8_t type[2];
    assert(nx_net_mbuf_copydata(m, 12, 2, type));
    if (nx_net_get16(type) == NX_NET_ETHERTYPE_IP) {
        assert(sent < FRAMES && length - 14 <= sizeof(frames[0]));
        assert(nx_net_mbuf_copydata(m, 14, length - 14, frames[sent]));
        frame_lengths[sent++] = length - 14;
    }
    nx_net_mbuf_freem(m);
    return true;
}
static const struct nx_net_if_ops fake = { NULL, NULL, fake_transmit };
static const uint8_t mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static const uint8_t peer_mac[6] = {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02};
static struct nx_net_if lo, em;
static void changed(struct nx_net_if *ifp) { nx_net_arp_if_changed(ifp); nx_net_ip_if_changed(ifp); }
static void kick(void *a) { (void)a; }

static uint16_t sum(const uint8_t *bytes, size_t length)
{
    struct nx_net_checksum c;
    nx_net_checksum_init(&c);
    nx_net_checksum_update(&c, bytes, length);
    return nx_net_checksum_finish(&c);
}
static uint16_t tcp_sum(nx_net_addr_t src, nx_net_addr_t dst, const uint8_t *tcp, size_t length)
{
    struct nx_net_checksum c;
    nx_net_checksum_init(&c);
    nx_net_checksum_pseudo(&c, src, dst, NX_NET_IPPROTO_TCP, (uint16_t)length);
    nx_net_checksum_update(&c, tcp, length);
    return nx_net_checksum_finish(&c);
}
static uint8_t pattern(size_t i) { return (uint8_t)(i * 7 + 3); }
static uint8_t data[200000];
static struct nx_net_mbuf *chain(const void *bytes, size_t length)
{
    struct nx_net_mbuf *m = nx_net_mbuf_get(&pool, true);
    assert(m && nx_net_mbuf_append(m, bytes, length));
    return m;
}

/* A segment from PEER:sport to ME:dport carrying data[offset..offset+len). */
static uint8_t raw[1600];
static size_t build(nx_net_port_t sport, nx_net_port_t dport, uint32_t seq, uint32_t ack, uint8_t flags,
                    uint16_t window, size_t offset, size_t length, uint16_t mss)
{
    size_t hl = mss ? 24 : 20, total = 20 + hl + length;
    memset(raw, 0, 20 + hl);
    raw[0] = 0x45;
    nx_net_put16(raw + 2, (uint16_t)total);
    raw[8] = 64;
    raw[9] = NX_NET_IPPROTO_TCP;
    nx_net_put32(raw + 12, PEER);
    nx_net_put32(raw + 16, ME);
    nx_net_put16(raw + 10, sum(raw, 20));
    uint8_t *t = raw + 20;
    nx_net_put16(t, sport); nx_net_put16(t + 2, dport);
    nx_net_put32(t + 4, seq); nx_net_put32(t + 8, ack);
    t[12] = (uint8_t)(hl / 4 << 4);
    t[13] = flags;
    nx_net_put16(t + 14, window);
    if (mss) { t[20] = 2; t[21] = 4; nx_net_put16(t + 22, mss); }
    memcpy(t + hl, data + offset, length);
    nx_net_put16(t + 16, tcp_sum(PEER, ME, t, hl + length));
    return total;
}
static void segment(nx_net_port_t sport, nx_net_port_t dport, uint32_t seq, uint32_t ack, uint8_t flags,
                    uint16_t window, size_t offset, size_t length, uint16_t mss)
{
    nx_net_ip_input(&em, chain(raw, build(sport, dport, seq, ack, flags, window, offset, length, mss)));
}
struct parsed { uint8_t flags; uint32_t seq, ack; uint16_t window, sport, dport, mss; size_t length; const uint8_t *data; };
static struct parsed frame(size_t i)
{
    assert(i < sent);
    const uint8_t *h = frames[i], *t = h + 20;
    assert(h[9] == NX_NET_IPPROTO_TCP && !sum(h, 20) && nx_net_get32(h + 12) == ME && nx_net_get32(h + 16) == PEER);
    size_t hl = (size_t)(t[12] >> 4) * 4;
    assert(!tcp_sum(ME, PEER, t, frame_lengths[i] - 20));
    struct parsed p = { t[13], nx_net_get32(t + 4), nx_net_get32(t + 8), nx_net_get16(t + 14), nx_net_get16(t),
                        nx_net_get16(t + 2), 0, frame_lengths[i] - 20 - hl, t + hl };
    if (hl == 24 && t[20] == 2) p.mss = nx_net_get16(t + 22);
    return p;
}
static struct parsed last(void) { return frame(sent - 1); }
static void advance(uint64_t ms)
{
    for (uint64_t done = 0; done < ms; done += NX_NET_TCP_TIMER_MS) {
        now += NX_NET_TCP_TIMER_MS;
        nx_net_tcp_timer();
    }
}
static unsigned events;
static void count(void *a) { (void)a; ++events; }

/* An established connection to PEER:80 with window and peer MSS 1000. */
#define PEER_ISS 5000u
static struct nx_net_tcp *established(nx_net_port_t port, uint16_t window)
{
    struct nx_net_tcp *t;
    assert(nx_net_tcp_open(count, NULL, &t) == NX_NET_OK && nx_net_tcp_connect(t, PEER, port) == NX_NET_OK);
    struct parsed s = last();
    assert(s.flags == SYN && s.mss == 1460 && t->state == NX_NET_TCP_SYN_SENT);
    segment(port, t->local_port, PEER_ISS, s.seq + 1, SYN | ACK, window, 0, 0, 1000);
    assert(t->state == NX_NET_TCP_ESTABLISHED && t->mss == 1000 && last().flags == ACK && last().ack == PEER_ISS + 1);
    return t;
}
/* Refresh the gateway's ARP entry; long fake waits outlive its 20 minutes. */
static void resolve(void)
{
    uint8_t arp[28] = {0, 1, 8, 0, 6, 4, 0, 2};
    memcpy(arp + 8, peer_mac, 6); nx_net_put32(arp + 14, PEER); memcpy(arp + 18, mac, 6); nx_net_put32(arp + 24, ME);
    nx_net_arp_input(&em, chain(arp, sizeof(arp)));
}
static size_t drain(struct nx_net_tcp *t, uint8_t *out, size_t max)
{
    size_t total = 0, n;
    while (total < max && nx_net_tcp_read(t, out + total, max - total, &n) == NX_NET_OK && n) total += n;
    return total;
}

static void active_open(void)
{
    resolve();
    struct nx_net_tcp_stats before = nx_net_tcp_stats();
    sent = 0;
    events = 0;
    struct nx_net_tcp *t = established(80, 8000);
    struct parsed syn = frame(0);
    assert(syn.window == NX_NET_TCP_RCVBUF && syn.sport >= NX_NET_TCP_EPHEMERAL_FIRST && syn.dport == 80);
    assert(frame(1).seq == syn.seq + 1 && events == 1);
    assert(nx_net_tcp_connect(t, PEER, 80) == NX_NET_INVALID);
    nx_net_tcp_abort(t);
    assert(last().flags == (RST | ACK) && last().seq == syn.seq + 1);

    /* A reset answering our SYN is refusal; an unacceptable ACK draws a reset. */
    assert(nx_net_tcp_open(count, NULL, &t) == NX_NET_OK && nx_net_tcp_connect(t, PEER, 81) == NX_NET_OK);
    syn = last();
    segment(81, t->local_port, 0, syn.seq + 5, ACK, 0, 0, 0, 0);
    assert(last().flags == RST && last().seq == syn.seq + 5 && t->state == NX_NET_TCP_SYN_SENT);
    segment(81, t->local_port, 0, syn.seq + 1, RST, 0, 0, 0, 0);   /* no ACK: ignored */
    assert(t->state == NX_NET_TCP_SYN_SENT);
    segment(81, t->local_port, 0, syn.seq + 1, RST | ACK, 0, 0, 0, 0);
    size_t n;
    assert(t->state == NX_NET_TCP_CLOSED && nx_net_tcp_read(t, data, 1, &n) == NX_NET_REFUSED);
    assert(nx_net_tcp_take_error(t) == NX_NET_REFUSED && nx_net_tcp_take_error(t) == NX_NET_OK);
    nx_net_tcp_close(t);

    /* An unanswered SYN is retried after 1, 2, 4, 8, 16 and 32 s, then fails. */
    assert(nx_net_tcp_open(count, NULL, &t) == NX_NET_OK && nx_net_tcp_connect(t, PEER, 82) == NX_NET_OK);
    syn = last();
    size_t first = sent;
    static const uint64_t waits[] = {1000, 2000, 4000, 8000, 16000, 32000};
    for (unsigned i = 0; i < 6; ++i) {
        advance(waits[i] - NX_NET_TCP_TIMER_MS);
        assert(sent == first + i);
        advance(NX_NET_TCP_TIMER_MS);
        assert(sent == first + i + 1 && last().flags == SYN && last().seq == syn.seq);
    }
    advance(63999);
    assert(t->state == NX_NET_TCP_CLOSED && nx_net_tcp_take_error(t) == NX_NET_TIMED_OUT && sent == first + 6);
    nx_net_tcp_close(t);
    struct nx_net_tcp_stats s = nx_net_tcp_stats();
    assert(s.timeouts == before.timeouts + 1 && s.retransmits >= before.retransmits + 6 && s.connects == before.connects + 3);
    assert(nx_net_tcp_used() == 0 && pool.stats.mbufs == 0);
}

static void passive_open(void)
{
    resolve();
    struct nx_net_tcp *l, *c, *extra;
    sent = 0;
    events = 0;
    assert(nx_net_tcp_open(count, NULL, &l) == NX_NET_OK && nx_net_tcp_bind(l, 0, 8080, false) == NX_NET_OK);
    assert(nx_net_tcp_listen(l, 2) == NX_NET_OK && nx_net_tcp_accept(l, NULL, NULL, &c) == NX_NET_AGAIN);
    segment(3000, 8080, 100, 0, SYN, 4000, 0, 0, 1200);
    struct parsed sa = last();
    assert(sa.flags == (SYN | ACK) && sa.ack == 101 && sa.mss == 1460 && nx_net_tcp_used() == 2);
    /* Our SYN-ACK was lost: the repeated SYN brings it back. */
    segment(3000, 8080, 100, 0, SYN, 4000, 0, 0, 1200);
    assert(sent == 2 && last().flags == (SYN | ACK) && last().seq == sa.seq);
    assert(nx_net_tcp_accept(l, NULL, NULL, &c) == NX_NET_AGAIN);
    segment(3000, 8080, 101, sa.seq + 1, ACK, 4000, 0, 0, 0);
    assert(events == 1 && nx_net_tcp_accept(l, count, NULL, &c) == NX_NET_OK);
    assert(c->state == NX_NET_TCP_ESTABLISHED && c->mss == 1200 && c->foreign_port == 3000 && c->local_addr == ME);
    assert(nx_net_tcp_accept(l, NULL, NULL, &extra) == NX_NET_AGAIN);
    /* Backlog 2: two embryonic connections, then SYNs are dropped. */
    segment(3001, 8080, 200, 0, SYN, 4000, 0, 0, 0);
    segment(3002, 8080, 300, 0, SYN, 4000, 0, 0, 0);
    size_t before = sent;
    segment(3003, 8080, 400, 0, SYN, 4000, 0, 0, 0);
    assert(sent == before && nx_net_tcp_stats().listen_drops >= 1 && nx_net_tcp_used() == 4);
    /* An embryonic connection that is never completed times out after its SYN-ACK retries. */
    advance(130000);
    assert(nx_net_tcp_used() == 2);
    /* ACK without SYN to a listener, or anything to a closed port, is reset. */
    segment(3004, 8080, 500, 777, ACK, 4000, 0, 0, 0);
    assert(last().flags == RST && last().seq == 777);
    segment(3004, 9999, 500, 0, SYN, 4000, 0, 0, 0);
    assert(last().flags == (RST | ACK) && last().seq == 0 && last().ack == 501);
    segment(3004, 9999, 500, 0, FIN | ACK, 4000, 0, 10, 0);
    assert(last().flags == RST && last().seq == 0);
    before = sent;
    segment(3004, 9999, 500, 0, RST, 4000, 0, 0, 0);
    assert(sent == before);
    /* Closing a listener resets its pending children and frees it. */
    segment(3005, 8080, 600, 0, SYN, 4000, 0, 0, 0);
    nx_net_tcp_close(l);
    assert(last().flags == (RST | ACK) && last().dport == 3005 && nx_net_tcp_used() == 1);
    nx_net_tcp_abort(c);
    assert(nx_net_tcp_used() == 0 && pool.stats.mbufs == 0);
}

static void transfer(void)
{
    resolve();
    sent = 0;
    struct nx_net_tcp *t = established(90, 8000);
    uint32_t base = last().seq;
    size_t n;
    for (size_t i = 0; i < sizeof(data); ++i) data[i] = pattern(i);
    /* Nagle: full segments go at once; the 500-byte tail waits for an ACK. */
    sent = 0;
    assert(nx_net_tcp_write(t, data, 2500, &n) == NX_NET_OK && n == 2500);
    assert(sent == 2 && frame(0).length == 1000 && frame(1).length == 1000 && frame(1).seq == base + 1000);
    assert(!memcmp(frame(1).data, data + 1000, 1000) && !(frame(0).flags & PSH));
    segment(90, t->local_port, PEER_ISS + 1, base + 2000, ACK, 8000, 0, 0, 0);
    assert(sent == 3 && frame(2).length == 500 && (frame(2).flags & PSH) && frame(2).seq == base + 2000);
    segment(90, t->local_port, PEER_ISS + 1, base + 2500, ACK, 8000, 0, 0, 0);
    nx_net_tcp_set_nodelay(t, true);
    assert(nx_net_tcp_write(t, data, 10, &n) == NX_NET_OK && sent == 4 && last().length == 10);
    segment(90, t->local_port, PEER_ISS + 1, base + 2510, ACK, 8000, 0, 0, 0);
    base += 2510;
    assert(t->snd_una == base && t->send.cc == 0 && t->srtt);

    /* Retransmission backs off and stops at the ACK. */
    uint32_t rto = t->rto;
    sent = 0;
    assert(nx_net_tcp_write(t, data, 300, &n) == NX_NET_OK && sent == 1);
    advance(rto);
    assert(sent == 2 && frame(1).seq == base && frame(1).length == 300);
    advance(2 * rto);
    assert(sent == 3 && frame(2).seq == base);
    segment(90, t->local_port, PEER_ISS + 1, base + 300, ACK, 8000, 0, 0, 0);
    advance(10000);
    assert(sent == 3 && !t->rexmt_at && t->rxtshift == 0);
    base += 300;

    /* Three duplicate ACKs retransmit at once. */
    sent = 0;
    assert(nx_net_tcp_write(t, data, 4000, &n) == NX_NET_OK && n == 4000 && sent >= 2);
    size_t burst = sent;
    for (unsigned i = 0; i < 3; ++i) segment(90, t->local_port, PEER_ISS + 1, base, ACK, 8000, 0, 0, 0);
    assert(sent == burst + 1 && last().seq == base && last().length == 1000 && nx_net_tcp_stats().fast_retransmits == 1);
    segment(90, t->local_port, PEER_ISS + 1, base + 4000, ACK, 8000, 0, 0, 0);
    advance(5000);
    for (unsigned guard = 0; t->send.cc && guard < 10; ++guard) {   /* whatever followed */
        segment(90, t->local_port, PEER_ISS + 1, t->snd_max, ACK, 8000, 0, 0, 0);
        advance(t->rto);
    }
    assert(t->send.cc == 0);
    base = t->snd_una;

    /* A zero window is probed after 5 s with one byte; data follows when it opens. */
    segment(90, t->local_port, PEER_ISS + 1, base, ACK, 0, 0, 0, 0);
    sent = 0;
    assert(nx_net_tcp_write(t, data, 3000, &n) == NX_NET_OK && sent == 0 && t->persist_at);
    advance(NX_NET_TCP_PERSIST_MIN_MS);
    assert(sent == 1 && last().length == 1 && last().seq == base && nx_net_tcp_stats().persist_probes == 1);
    segment(90, t->local_port, PEER_ISS + 1, base, ACK, 0, 0, 0, 0);   /* still closed */
    advance(2 * NX_NET_TCP_PERSIST_MIN_MS);
    assert(sent == 2 && last().length == 1 && last().seq == base);
    segment(90, t->local_port, PEER_ISS + 1, base + 1, ACK, 8000, 0, 0, 0);
    assert(sent >= 3 && frame(2).seq == base + 1 && !t->persist_at);
    for (unsigned guard = 0; t->send.cc && guard < 10; ++guard)
        segment(90, t->local_port, PEER_ISS + 1, t->snd_max, ACK, 8000, 0, 0, 0);
    assert(t->send.cc == 0 && t->snd_una == base + 3000);

    /* Receiving: in order, then reordered and duplicated segments. */
    uint32_t rs = PEER_ISS + 1;
    sent = 0;
    segment(90, t->local_port, rs, t->snd_max, ACK | PSH, 8000, 0, 100, 0);
    assert(last().ack == rs + 100 && nx_net_tcp_readable(t) == 100);
    segment(90, t->local_port, rs + 300, t->snd_max, ACK, 8000, 300, 100, 0);   /* hole at 100..300 */
    assert(last().ack == rs + 100 && t->reass_count == 1 && nx_net_tcp_readable(t) == 100);
    segment(90, t->local_port, rs + 200, t->snd_max, ACK, 8000, 200, 150, 0);   /* overlaps the held one */
    assert(last().ack == rs + 100 && t->reass_count == 2);
    segment(90, t->local_port, rs + 50, t->snd_max, ACK, 8000, 50, 150, 0);     /* half old, half new */
    assert(last().ack == rs + 400 && t->reass_count == 0 && nx_net_tcp_readable(t) == 400);
    segment(90, t->local_port, rs, t->snd_max, ACK, 8000, 0, 100, 0);           /* duplicate */
    assert(last().ack == rs + 400 && nx_net_tcp_readable(t) == 400);
    static uint8_t got[70000];
    assert(drain(t, got, sizeof(got)) == 400 && !memcmp(got, data, 400));
    assert(nx_net_tcp_read(t, got, 1, &n) == NX_NET_AGAIN);
    rs += 400;
    struct nx_net_tcp_stats st = nx_net_tcp_stats();
    assert(st.reordered >= 2 && st.duplicates >= 2);

    /* Filling the receive buffer closes the window; reading reopens it. */
    for (size_t off = 0; off < NX_NET_TCP_RCVBUF + 1000; off += 1000)
        segment(90, t->local_port, rs + (uint32_t)off, t->snd_max, ACK, 8000, off, 1000, 0);
    assert(nx_net_tcp_readable(t) == NX_NET_TCP_RCVBUF && last().window == 0 && last().ack == rs + NX_NET_TCP_RCVBUF);
    assert(nx_net_tcp_stats().out_of_window > st.out_of_window);
    sent = 0;
    assert(nx_net_tcp_read(t, got, 8000, &n) == NX_NET_OK && n == 8000 && sent == 1 && last().window == 8000);
    assert(drain(t, got + 8000, sizeof(got)) == NX_NET_TCP_RCVBUF - 8000 && !memcmp(got, data, NX_NET_TCP_RCVBUF));
    rs += NX_NET_TCP_RCVBUF;

    /* The send buffer is bounded. */
    segment(90, t->local_port, rs, t->snd_max, ACK, 0, 0, 0, 0);
    assert(nx_net_tcp_write(t, data, sizeof(data), &n) == NX_NET_OK && n == NX_NET_TCP_SNDBUF);
    assert(nx_net_tcp_writable(t) == 0 && nx_net_tcp_write(t, data, 1, &n) == NX_NET_AGAIN && n == 0);

    /* Malformed segments are dropped unanswered. */
    st = nx_net_tcp_stats();
    sent = 0;
    size_t length = build(90, t->local_port, rs, t->snd_max, ACK, 8000, 0, 10, 0);
    raw[45] ^= 1;
    nx_net_ip_input(&em, chain(raw, length));
    length = build(90, t->local_port, rs, t->snd_max, ACK, 8000, 0, 10, 0);
    raw[32] = 0x40;                                                   /* offset 16 bytes */
    nx_net_ip_input(&em, chain(raw, length));
    raw[32] = 0xf0;                                                   /* 60 > 30 bytes */
    nx_net_ip_input(&em, chain(raw, length));
    length = build(90, t->local_port, rs, t->snd_max, ACK, 8000, 0, 0, 0);
    nx_net_put16(raw + 2, 39); nx_net_put16(raw + 10, 0); nx_net_put16(raw + 10, sum(raw, 20));
    nx_net_ip_input(&em, chain(raw, 39));
    struct nx_net_tcp_stats s2 = nx_net_tcp_stats();
    assert(sent == 0 && s2.bad_checksum == st.bad_checksum + 1 && s2.bad_offset == st.bad_offset + 2 &&
           s2.too_short == st.too_short + 1 && nx_net_tcp_readable(t) == 0);

    /* RFC 5961: a SYN or an inexact in-window reset draws a challenge ACK. */
    segment(90, t->local_port, rs, 0, SYN, 8000, 0, 0, 0);
    segment(90, t->local_port, rs + 10, 0, RST, 8000, 0, 0, 0);
    assert(sent == 2 && frame(0).flags == ACK && frame(1).flags == ACK && t->state == NX_NET_TCP_ESTABLISHED);
    segment(90, t->local_port, rs + 100000, 0, RST, 8000, 0, 0, 0);   /* outside: ignored */
    assert(sent == 2);
    events = 0;
    segment(90, t->local_port, rs, 0, RST, 8000, 0, 0, 0);
    assert(t->state == NX_NET_TCP_CLOSED && events && nx_net_tcp_read(t, got, 1, &n) == NX_NET_RESET);
    assert(nx_net_tcp_write(t, data, 1, &n) == NX_NET_RESET && t->send.cc == 0);
    nx_net_tcp_close(t);
    assert(nx_net_tcp_used() == 0 && pool.stats.mbufs == 0);
}

static void teardown(void)
{
    resolve();
    size_t n;
    uint8_t buffer[16];
    /* Active close: FIN, its ACK, the peer's FIN, TIME_WAIT for 60 s. */
    sent = 0;
    struct nx_net_tcp *t = established(100, 8000);
    uint32_t base = last().seq, rs = PEER_ISS + 1;
    assert(nx_net_tcp_shutdown(t) == NX_NET_OK && t->state == NX_NET_TCP_FIN_WAIT_1 && last().flags == (FIN | ACK));
    assert(nx_net_tcp_write(t, data, 1, &n) == NX_NET_INVALID);
    advance(t->rto);
    assert(last().flags == (FIN | ACK) && last().seq == base);                    /* lost FIN resent */
    segment(100, t->local_port, rs, base + 1, ACK, 8000, 0, 0, 0);
    assert(t->state == NX_NET_TCP_FIN_WAIT_2);
    segment(100, t->local_port, rs, base + 1, ACK | PSH, 8000, 0, 5, 0);          /* half-closed: still reads */
    assert(nx_net_tcp_read(t, buffer, 16, &n) == NX_NET_OK && n == 5);
    segment(100, t->local_port, rs + 5, base + 1, FIN | ACK, 8000, 0, 0, 0);
    assert(t->state == NX_NET_TCP_TIME_WAIT && last().flags == ACK && last().ack == rs + 6);
    assert(nx_net_tcp_read(t, buffer, 16, &n) == NX_NET_OK && n == 0);             /* end of stream */
    advance(59000);
    size_t before = sent;
    segment(100, t->local_port, rs + 5, base + 1, FIN | ACK, 8000, 0, 0, 0);      /* retransmitted FIN */
    assert(sent == before + 1 && last().ack == rs + 6 && t->state == NX_NET_TCP_TIME_WAIT);
    advance(59000);
    assert(t->state == NX_NET_TCP_TIME_WAIT);
    advance(1000);
    assert(t->state == NX_NET_TCP_CLOSED && nx_net_tcp_take_error(t) == NX_NET_OK);
    nx_net_tcp_close(t);

    /* Passive close: the peer's FIN gives end of stream; our close sends FIN
     * and LAST_ACK frees the block when acknowledged. */
    t = established(101, 8000);
    base = last().seq;
    segment(101, t->local_port, rs, base, FIN | ACK, 8000, 0, 0, 0);
    assert(t->state == NX_NET_TCP_CLOSE_WAIT && last().ack == rs + 1);
    assert(nx_net_tcp_read(t, buffer, 16, &n) == NX_NET_OK && n == 0);
    assert(nx_net_tcp_write(t, data, 3, &n) == NX_NET_OK && n == 3);              /* half-close: may still send */
    nx_net_tcp_close(t);
    assert(frame(sent - 2).length == 3 && last().flags == (FIN | ACK) && last().seq == base + 3 && nx_net_tcp_used() == 1);
    segment(101, t->local_port, rs + 1, base + 4, ACK, 8000, 0, 0, 0);
    assert(nx_net_tcp_used() == 0);

    /* Simultaneous close: CLOSING, then TIME_WAIT. */
    t = established(102, 8000);
    base = last().seq;
    assert(nx_net_tcp_shutdown(t) == NX_NET_OK);
    segment(102, t->local_port, rs, base, FIN | ACK, 8000, 0, 0, 0);
    assert(t->state == NX_NET_TCP_CLOSING);
    segment(102, t->local_port, rs + 1, base + 1, ACK, 8000, 0, 0, 0);
    assert(t->state == NX_NET_TCP_TIME_WAIT);
    nx_net_tcp_close(t);
    advance(2 * NX_NET_TCP_MSL_MS);
    assert(nx_net_tcp_used() == 0);

    /* Closing with unread data resets the peer (RFC 2525). */
    t = established(103, 8000);
    base = last().seq;
    segment(103, t->local_port, rs, base, ACK, 8000, 0, 10, 0);
    nx_net_tcp_close(t);
    assert(last().flags == (RST | ACK) && nx_net_tcp_used() == 0);

    /* After close, a peer that never sends FIN is dropped from FIN_WAIT_2,
     * and data arriving after close resets it. */
    t = established(104, 8000);
    base = last().seq;
    nx_net_tcp_close(t);
    segment(104, t->local_port, rs, base + 1, ACK, 8000, 0, 0, 0);
    assert(t->state == NX_NET_TCP_FIN_WAIT_2 && nx_net_tcp_used() == 1);
    advance(NX_NET_TCP_FIN_WAIT_2_MS);
    assert(nx_net_tcp_used() == 0);
    t = established(105, 8000);
    base = last().seq;
    nx_net_tcp_close(t);
    segment(105, t->local_port, rs, base + 1, ACK, 8000, 0, 4, 0);
    assert(last().flags == (RST | ACK) && nx_net_tcp_used() == 0);

    /* An established connection whose data is never acknowledged gives up
     * after 12 retransmissions. */
    t = established(106, 8000);
    assert(nx_net_tcp_write(t, data, 10, &n) == NX_NET_OK);
    advance(12 * 64000);
    assert(t->state == NX_NET_TCP_CLOSED && nx_net_tcp_take_error(t) == NX_NET_TIMED_OUT && last().flags == (RST | ACK));
    nx_net_tcp_close(t);
    assert(nx_net_tcp_used() == 0 && pool.stats.mbufs == 0);
}

static void icmp_errors(void)
{
    resolve();
    /* Quote our datagram in an ICMP error from the peer. */
    struct nx_net_tcp *t;
    assert(nx_net_tcp_open(NULL, NULL, &t) == NX_NET_OK && nx_net_tcp_connect(t, PEER, 110) == NX_NET_OK);
    uint8_t quoted[28], message[56];
    memcpy(quoted, frames[sent - 1], 28);
    uint8_t body[36] = { 3, 3 };
    memcpy(body + 8, quoted, 28);
    nx_net_put16(body + 2, sum(body, 36));
    memset(message, 0, 20);
    message[0] = 0x45; nx_net_put16(message + 2, 56); message[8] = 64; message[9] = 1;
    nx_net_put32(message + 12, PEER); nx_net_put32(message + 16, ME);
    nx_net_put16(message + 10, sum(message, 20));
    memcpy(message + 20, body, 36);
    nx_net_ip_input(&em, chain(message, 56));
    assert(t->state == NX_NET_TCP_CLOSED && nx_net_tcp_take_error(t) == NX_NET_REFUSED);
    nx_net_tcp_close(t);
    /* Needfrag lowers the MSS and resends. */
    t = established(111, 8000);
    size_t n;
    nx_net_tcp_set_nodelay(t, true);
    assert(nx_net_tcp_write(t, data, 1000, &n) == NX_NET_OK && last().length == 1000);
    memcpy(body + 8, frames[sent - 1], 28);
    body[1] = 4; nx_net_put16(body + 6, 576);
    nx_net_put16(body + 2, 0); nx_net_put16(body + 2, sum(body, 36));
    memcpy(message + 20, body, 36);
    size_t before = sent;
    nx_net_ip_input(&em, chain(message, 56));
    assert(t->mss == 536 && sent == before + 2 && frame(before).length == 536 && frame(before).seq == frame(before - 1).seq);
    assert(last().length == 464 && last().seq == frame(before).seq + 536);
    /* Host unreachable is soft: reported only if the connection then times out. */
    body[1] = 1; nx_net_put16(body + 6, 0);
    memcpy(body + 8, frames[sent - 1], 28);
    nx_net_put16(body + 2, 0); nx_net_put16(body + 2, sum(body, 36));
    memcpy(message + 20, body, 36);
    nx_net_ip_input(&em, chain(message, 56));
    assert(t->state == NX_NET_TCP_ESTABLISHED && t->error == NX_NET_OK);
    advance(12 * 64000);
    assert(nx_net_tcp_take_error(t) == NX_NET_HOST_UNREACHABLE);
    nx_net_tcp_close(t);
    assert(nx_net_tcp_used() == 0 && pool.stats.mbufs == 0);
}

static void limits(void)
{
    resolve();
    struct nx_net_tcp *all[NX_NET_TCP_SOCKETS], *t;
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i) assert(nx_net_tcp_open(NULL, NULL, &all[i]) == NX_NET_OK);
    assert(nx_net_tcp_open(NULL, NULL, &t) == NX_NET_FULL);
    assert(nx_net_tcp_bind(all[0], 0, 22, false) == NX_NET_DENIED && nx_net_tcp_bind(all[0], 0, 22, true) == NX_NET_OK);
    assert(nx_net_tcp_bind(all[1], ME, 22, true) == NX_NET_EXISTS && nx_net_tcp_bind(all[1], IP(1, 2, 3, 4), 23, true) == NX_NET_INVALID);
    nx_net_tcp_set_reuse(all[0], true); nx_net_tcp_set_reuse(all[1], true);
    assert(nx_net_tcp_bind(all[1], ME, 22, true) == NX_NET_OK && nx_net_tcp_listen(all[1], 100) == NX_NET_OK && all[1]->backlog == 8);
    assert(nx_net_tcp_connect(all[1], PEER, 1) == NX_NET_INVALID && nx_net_tcp_connect(all[2], IP(224, 0, 0, 1), 1) == NX_NET_INVALID);
    assert(nx_net_tcp_connect(all[2], PEER, 0) == NX_NET_INVALID);
    size_t n;
    assert(nx_net_tcp_read(all[2], data, 1, &n) == NX_NET_NOT_CONNECTED && nx_net_tcp_write(all[2], data, 1, &n) == NX_NET_NOT_CONNECTED);
    /* A full table refuses passive opens. */
    segment(4000, 22, 1, 0, SYN, 1000, 0, 0, 0);
    assert(nx_net_tcp_stats().listen_drops >= 1);
    for (unsigned i = 0; i < NX_NET_TCP_SOCKETS; ++i) nx_net_tcp_close(all[i]);
    assert(nx_net_tcp_used() == 0);
}

/* Loopback transfer with deterministic loss and reordering of lo0 packets. */
/* A swapped packet is held until the next one, possibly in a later batch,
 * has been delivered. */
static struct nx_net_mbuf *held;
static void pump(unsigned *counter, unsigned drop_every, unsigned swap_every)
{
    struct nx_net_mbuf *list = lo.queue_head;
    lo.queue_head = lo.queue_tail = NULL;
    lo.queue_length = 0;
    if (!list && held && !swap_every) { nx_net_ip_input(&lo, held); held = NULL; }
    while (list) {
        struct nx_net_mbuf *m = list;
        list = m->nextpkt;
        m->nextpkt = NULL;
        nx_net_mbuf_adj_head(m, 2);   /* the queued ethertype */
        unsigned k = ++*counter;
        if (drop_every && k % drop_every == 0) { nx_net_mbuf_freem(m); continue; }
        if (swap_every && k % swap_every == 0 && !held) { held = m; continue; }
        nx_net_ip_input(&lo, m);
        if (held) { nx_net_ip_input(&lo, held); held = NULL; }
    }
}
static void loopback(unsigned drop_every, unsigned swap_every, size_t total)
{
    struct nx_net_tcp *l, *client, *server = NULL;
    struct nx_net_tcp_stats before = nx_net_tcp_stats();
    assert(nx_net_tcp_open(NULL, NULL, &l) == NX_NET_OK && nx_net_tcp_bind(l, LOCALHOST, 7000, false) == NX_NET_OK);
    assert(nx_net_tcp_listen(l, 1) == NX_NET_OK);
    assert(nx_net_tcp_open(NULL, NULL, &client) == NX_NET_OK && nx_net_tcp_connect(client, LOCALHOST, 7000) == NX_NET_OK);
    static uint8_t got[200000], back[200000];
    size_t written = 0, received = 0, echoed = 0, returned = 0, n;
    unsigned counter = 0;
    bool client_shut = false, server_shut = false;
    for (unsigned round = 0; round < 20000; ++round) {
        pump(&counter, drop_every, swap_every);
        if (!server) { if (nx_net_tcp_accept(l, NULL, NULL, &server) == NX_NET_OK) nx_net_tcp_close(l); }
        if (client->state == NX_NET_TCP_ESTABLISHED && written < total &&
            nx_net_tcp_write(client, data + written, total - written, &n) == NX_NET_OK) written += n;
        if (written == total && !client_shut) { assert(nx_net_tcp_shutdown(client) == NX_NET_OK); client_shut = true; }
        if (server) {
            while (nx_net_tcp_read(server, got + received, sizeof(got) - received, &n) == NX_NET_OK && n) received += n;
            /* Echo back what arrived. */
            if (echoed < received && nx_net_tcp_write(server, got + echoed, received - echoed, &n) == NX_NET_OK) echoed += n;
            if (server->fin_received && echoed == received && !server_shut) { nx_net_tcp_shutdown(server); server_shut = true; }
        }
        while (nx_net_tcp_read(client, back + returned, sizeof(back) - returned, &n) == NX_NET_OK && n) returned += n;
        if (returned == total && server && server->state == NX_NET_TCP_CLOSED && client->state == NX_NET_TCP_TIME_WAIT) break;
        if (!lo.queue_head) {
            if (held) { nx_net_ip_input(&lo, held); held = NULL; }
            else advance(NX_NET_TCP_TIMER_MS);
        }
    }
    assert(received == total && !memcmp(got, data, total) && returned == total && !memcmp(back, data, total));
    assert(client->state == NX_NET_TCP_TIME_WAIT && server->state == NX_NET_TCP_CLOSED);
    struct nx_net_tcp_stats after = nx_net_tcp_stats();
    if (drop_every) assert(after.retransmits > before.retransmits);
    if (swap_every) assert(after.reordered > before.reordered);
    if (!drop_every && !swap_every) assert(after.retransmits == before.retransmits && after.reordered == before.reordered);
    printf("loopback drop %u swap %u: %zu bytes each way, %llu retransmits (%llu fast), %llu reordered\n", drop_every,
           swap_every, total, (unsigned long long)(after.retransmits - before.retransmits),
           (unsigned long long)(after.fast_retransmits - before.fast_retransmits),
           (unsigned long long)(after.reordered - before.reordered));
    assert(nx_net_tcp_take_error(client) == NX_NET_OK && nx_net_tcp_take_error(server) == NX_NET_OK);
    nx_net_tcp_close(server);
    nx_net_tcp_close(client);
    advance(2 * NX_NET_TCP_MSL_MS);
    pump(&counter, 0, 0);
    assert(nx_net_tcp_used() == 0 && pool.stats.mbufs == 0 && pool.stats.clusters == 0);
}

int main(void)
{
    nx_net_budget_init(&budget, 2048);
    assert(nx_net_mbuf_pool_init(&pool, &ops, &budget, (struct nx_net_mbuf_limits){ 4000, 1500 }));
    nx_net_arp_init(&pool, clock_ms);
    nx_net_ip_init(&pool, clock_ms);
    nx_net_icmp_init();
    nx_net_tcp_init(&pool, clock_ms, 0x123456789abcdefull);
    nx_net_if_set_change_hook(changed);
    assert(nx_net_proto_register(NX_NET_ETHERTYPE_IP, nx_net_ip_input) == NX_NET_OK);
    assert(nx_net_ip_register(NX_NET_IPPROTO_ICMP, nx_net_icmp_input) == NX_NET_OK);
    assert(nx_net_ip_register(NX_NET_IPPROTO_TCP, nx_net_tcp_input) == NX_NET_OK);
    assert(nx_net_icmp_register_notify(NX_NET_IPPROTO_TCP, nx_net_tcp_notify) == NX_NET_OK);
    nx_net_loop_init(&lo, "lo0", kick, NULL);
    nx_net_ether_init(&em, "em0", mac, &fake, NULL);
    assert(nx_net_if_attach(&lo) == NX_NET_OK && nx_net_if_attach(&em) == NX_NET_OK);
    assert(nx_net_if_up(&lo) == NX_NET_OK && nx_net_if_up(&em) == NX_NET_OK);
    assert(nx_net_inet_set(&lo, LOCALHOST, IP(255, 0, 0, 0)) == NX_NET_OK);
    assert(nx_net_inet_set(&em, ME, IP(255, 255, 255, 0)) == NX_NET_OK);
    assert(nx_net_route_set_default(PEER) == NX_NET_OK);
    resolve();
    for (size_t i = 0; i < sizeof(data); ++i) data[i] = pattern(i);

    active_open();
    passive_open();
    transfer();
    for (size_t i = 0; i < sizeof(data); ++i) data[i] = pattern(i);
    teardown();
    icmp_errors();
    limits();
    loopback(0, 0, 150000);    /* 16344-byte segments */
    lo.mtu = 1500;             /* then Ethernet-sized ones, for many more packets */
    loopback(0, 0, 150000);
    loopback(7, 0, 150000);    /* loss */
    loopback(0, 3, 150000);    /* reordering */
    loopback(5, 4, 100000);    /* both */
    /* Initial sequence numbers differ per connection and over time. */
    resolve();
    sent = 0;
    struct nx_net_tcp *a, *b;
    assert(nx_net_tcp_open(NULL, NULL, &a) == NX_NET_OK && nx_net_tcp_open(NULL, NULL, &b) == NX_NET_OK);
    assert(nx_net_tcp_connect(a, PEER, 1) == NX_NET_OK && nx_net_tcp_connect(b, PEER, 2) == NX_NET_OK);
    nx_net_seq_t first = a->iss;
    assert(a->iss != b->iss && (uint32_t)(a->iss - b->iss) > 1000 && (uint32_t)(b->iss - a->iss) > 1000);
    nx_net_tcp_abort(a);
    now += 1000;
    assert(nx_net_tcp_open(NULL, NULL, &a) == NX_NET_OK && nx_net_tcp_bind(a, 0, b->local_port - 1, false) == NX_NET_OK &&
           nx_net_tcp_connect(a, PEER, 1) == NX_NET_OK && a->iss - first == 250000);   /* 4 us clock */
    nx_net_tcp_init(&pool, clock_ms, 42);
    assert(nx_net_tcp_used() == 0);
    nx_net_if_detach(&em);
    nx_net_if_detach(&lo);
    nx_net_route_clear_default();
    nx_net_arp_flush(NULL);
    nx_net_ip_flush();
    assert(pool.stats.mbufs == 0 && pool.stats.clusters == 0 && nx_net_mbuf_pool_destroy(&pool) && !live_pages);
    puts("PASS: TCP active/passive open, listen backlog, resets, retransmission backoff and timeouts, Nagle, fast retransmit, zero-window probes, reordering, windows, malformed segments, RFC 5961 challenges, FIN/TIME_WAIT teardown, ICMP errors, lossy/reordered loopback transfers and mbuf ownership");
    return 0;
}
