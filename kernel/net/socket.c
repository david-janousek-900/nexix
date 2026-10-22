#include "net/socket.h"
#include "net/kernel.h"
#include "net/worker.h"
#include "process.h"
#include <nexix/socket.h>
#include <nexix/errno.h>
#include <nexix/syscall.h>

/* Protocol results as native errors, for the context they arise in. */
static int errno_of(enum nx_net_result r)
{
    switch (r) {
    case NX_NET_OK: return 0;
    case NX_NET_DOWN: return -NX_ENETDOWN;
    case NX_NET_TOO_BIG: return -NX_EMSGSIZE;
    case NX_NET_NO_BUFFERS: case NX_NET_FULL: return -NX_ENOBUFS;
    case NX_NET_INVALID: return -NX_EINVAL;
    case NX_NET_EXISTS: return -NX_EADDRINUSE;
    case NX_NET_UNSUPPORTED: return -NX_EOPNOTSUPP;
    case NX_NET_UNREACHABLE: return -NX_ENETUNREACH;
    case NX_NET_HOST_UNREACHABLE: return -NX_EHOSTUNREACH;
    case NX_NET_REFUSED: return -NX_ECONNREFUSED;
    case NX_NET_DENIED: return -NX_EACCES;
    case NX_NET_AGAIN: return -NX_EAGAIN;
    case NX_NET_RESET: return -NX_ECONNRESET;
    case NX_NET_TIMED_OUT: return -NX_ETIMEDOUT;
    case NX_NET_NOT_CONNECTED: return -NX_ENOTCONN;
    }
    return -NX_EINVAL;
}

static size_t live;
static void socket_event(void *argument)
{
    struct nx_socket *s = argument;
    ++s->events;
    proc_wakeup(s);
}

/* One request to the worker: function, socket and operands. */
struct request {
    struct nx_socket *s, *child;
    struct nx_net_mbuf *chain;
    int type, how, backlog, level, name;
    bool privileged, peer;
    nx_net_addr_t addr;
    nx_net_port_t port;
    const void *in;
    void *out;
    size_t length;
    int64_t result;
};
static int64_t call(void (*function)(void *), struct request *r)
{
    if (!net_worker_running() || !net_worker_call(function, r)) return -NX_ENOBUFS;
    return r->result;
}

/* Start the stack for the first socket; lo0 gets its loopback address. */
static void loopback(void *argument)
{
    struct request *r = argument;
    struct nx_net_if *lo = nx_net_if_find("lo0");
    r->result = lo && nx_net_inet_set(lo, NX_INADDR_LOOPBACK, 0xff000000u) == NX_NET_OK ? 0 : -NX_ENETDOWN;
}
static int ensure_stack(void)
{
    if (net_pool()) return 0;
    if (!net_start()) return -NX_ENOBUFS;
    net_worker_detach();   /* a kernel service, not the caller's child */
    struct request r = {0};
    return (int)call(loopback, &r);
}

static void create(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = nx_net_slab_alloc(net_socket_slab());
    if (!s) { r->result = -NX_ENOBUFS; return; }
    /* A datagram/raw SO_SNDBUF caps each datagram, as in BSD (udp_sendspace, RIPSNDQ). */
    *s = (struct nx_socket){ .type = (uint8_t)r->type, .send_size = r->type == NX_SOCK_RAW ? 8192 : NX_NET_UDP_SNDMAX };
    enum nx_net_result result;
    if (r->type == NX_SOCK_STREAM) result = nx_net_tcp_open(socket_event, s, &s->pcb.tcp);
    else if (r->type == NX_SOCK_DGRAM) result = nx_net_udp_open(socket_event, s, &s->pcb.udp);
    else result = nx_net_raw_open(r->privileged, socket_event, s, &s->pcb.raw);
    if (result != NX_NET_OK) {
        nx_net_slab_free(net_socket_slab(), s);
        r->result = errno_of(result);
        return;
    }
    ++live;
    r->s = s;
    r->result = 0;
}
int net_socket_create(int type, bool privileged, struct nx_socket **out)
{
    int error = ensure_stack();
    if (error) return error;
    struct request r = { .type = type, .privileged = privileged };
    int64_t result = call(create, &r);
    if (!result) *out = r.s;
    return (int)result;
}

static void release(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    if (s->type == NX_SOCK_STREAM) nx_net_tcp_close(s->pcb.tcp);
    else if (s->type == NX_SOCK_DGRAM) nx_net_udp_close(s->pcb.udp);
    else nx_net_raw_close(s->pcb.raw);
    nx_net_slab_free(net_socket_slab(), s);
    --live;
    r->result = 0;
}
void net_socket_release(struct nx_socket *s)
{
    struct request r = { .s = s };
    if (call(release, &r)) panic("socket release lost its worker");
}
size_t net_socket_count(void) { return live; }

static void bind(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    enum nx_net_result result;
    if (s->type == NX_SOCK_STREAM) result = nx_net_tcp_bind(s->pcb.tcp, r->addr, r->port, r->privileged);
    else if (s->type == NX_SOCK_DGRAM) result = nx_net_udp_bind(s->pcb.udp, r->addr, r->port, r->privileged);
    else result = nx_net_raw_bind(s->pcb.raw, r->addr);
    /* A second bind, or binding a connected stream, is EINVAL as in BSD;
     * an address that is not ours is EADDRNOTAVAIL. */
    if (result == NX_NET_INVALID && r->addr && !nx_net_inet_local(r->addr)) r->result = -NX_EADDRNOTAVAIL;
    else r->result = errno_of(result);
    if (result == NX_NET_FULL) r->result = -NX_EADDRNOTAVAIL;   /* no ephemeral port */
}
int net_socket_bind(struct nx_socket *s, nx_net_addr_t addr, nx_net_port_t port, bool privileged)
{
    struct request r = { .s = s, .addr = addr, .port = port, .privileged = privileged };
    return (int)call(bind, &r);
}

static void listen(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    if (s->type != NX_SOCK_STREAM) { r->result = -NX_EOPNOTSUPP; return; }
    enum nx_net_tcp_state state = s->pcb.tcp->state;
    if (state == NX_NET_TCP_LISTEN) {   /* BSD lets listen change the backlog */
        s->pcb.tcp->backlog = r->backlog < 1 ? 1 : r->backlog > NX_SOMAXCONN ? NX_SOMAXCONN : (size_t)r->backlog;
        r->result = 0;
        return;
    }
    if (state != NX_NET_TCP_CLOSED || s->pcb.tcp->foreign_port) { r->result = -NX_EINVAL; return; }
    enum nx_net_result result = nx_net_tcp_listen(s->pcb.tcp, r->backlog < 0 ? 0 : (size_t)r->backlog);
    r->result = result == NX_NET_FULL ? -NX_EADDRNOTAVAIL : errno_of(result);
}
int net_socket_listen(struct nx_socket *s, int backlog)
{
    struct request r = { .s = s, .backlog = backlog };
    return (int)call(listen, &r);
}

static void accept(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    if (s->type != NX_SOCK_STREAM || s->pcb.tcp->state != NX_NET_TCP_LISTEN) { r->result = -NX_EINVAL; return; }
    struct nx_socket *c = nx_net_slab_alloc(net_socket_slab());
    if (!c) { r->result = -NX_ENOBUFS; return; }
    *c = (struct nx_socket){ .type = NX_SOCK_STREAM, .connected_once = true };
    enum nx_net_result result = nx_net_tcp_accept(s->pcb.tcp, socket_event, c, &c->pcb.tcp);
    if (result != NX_NET_OK) {
        nx_net_slab_free(net_socket_slab(), c);
        r->result = errno_of(result);
        return;
    }
    ++live;
    r->child = c;
    r->addr = c->pcb.tcp->foreign_addr;
    r->port = c->pcb.tcp->foreign_port;
    r->result = 0;
}
int net_socket_accept(struct nx_socket *s, struct nx_socket **child, nx_net_addr_t *addr, nx_net_port_t *port)
{
    struct request r = { .s = s };
    int64_t result = call(accept, &r);
    if (!result) { *child = r.child; *addr = r.addr; *port = r.port; }
    return (int)result;
}

static void connect(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    enum nx_net_result result;
    if (s->type == NX_SOCK_DGRAM) { r->result = errno_of(nx_net_udp_connect(s->pcb.udp, r->addr, r->port)); return; }
    if (s->type == NX_SOCK_RAW) { r->result = errno_of(nx_net_raw_connect(s->pcb.raw, r->addr)); return; }
    struct nx_net_tcp *t = s->pcb.tcp;
    if (s->connecting && t->state == NX_NET_TCP_SYN_SENT) { r->result = -NX_EALREADY; return; }
    if (t->state == NX_NET_TCP_LISTEN) { r->result = -NX_EOPNOTSUPP; return; }
    if (t->state == NX_NET_TCP_CLOSED && t->foreign_port && t->error != NX_NET_OK) {
        r->result = errno_of(nx_net_tcp_take_error(t));   /* a failed non-blocking connect */
        s->connecting = false;
        return;
    }
    if (t->state != NX_NET_TCP_CLOSED || t->foreign_port) { r->result = -NX_EISCONN; return; }
    if (!r->addr) { r->result = -NX_EAFNOSUPPORT; return; }   /* AF_UNSPEC: streams cannot disconnect */
    result = nx_net_tcp_connect(t, r->addr, r->port);
    if (result == NX_NET_EXISTS) r->result = -NX_EADDRINUSE;
    else if (result == NX_NET_FULL) r->result = -NX_EADDRNOTAVAIL;
    else if (result == NX_NET_INVALID) r->result = -NX_EADDRNOTAVAIL;   /* 0.0.0.0, multicast, reserved */
    else r->result = errno_of(result);
    s->connecting = r->result == 0;
}
int net_socket_connect(struct nx_socket *s, nx_net_addr_t addr, nx_net_port_t port)
{
    struct request r = { .s = s, .addr = addr, .port = port };
    return (int)call(connect, &r);
}
static void connected(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    struct nx_net_tcp *t = s->pcb.tcp;
    if (t->state == NX_NET_TCP_SYN_SENT) { r->result = -NX_EAGAIN; return; }
    s->connecting = false;
    if (t->state == NX_NET_TCP_CLOSED) {
        int error = errno_of(nx_net_tcp_take_error(t));
        r->result = error ? error : -NX_ECONNABORTED;
        return;
    }
    s->connected_once = true;
    r->result = 0;
}
int net_socket_connected(struct nx_socket *s)
{
    if (s->type != NX_SOCK_STREAM) return 0;
    struct request r = { .s = s };
    return (int)call(connected, &r);
}

static void send(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    struct nx_net_tcp *t = s->pcb.tcp;
    if (t->state == NX_NET_TCP_ESTABLISHED || t->state == NX_NET_TCP_CLOSE_WAIT) s->connected_once = true;
    if (t->error != NX_NET_OK) { r->result = errno_of(nx_net_tcp_take_error(t)); return; }   /* reported once */
    if (s->shut_write || (s->connected_once && nx_net_tcp_writable(t) == 0 &&
                          t->state != NX_NET_TCP_ESTABLISHED && t->state != NX_NET_TCP_CLOSE_WAIT)) {
        r->result = -NX_EPIPE;
        return;
    }
    size_t done;
    enum nx_net_result result = nx_net_tcp_write(t, r->in, r->length, &done);
    r->result = result == NX_NET_OK ? (int64_t)done : errno_of(result);
}
int64_t net_socket_send(struct nx_socket *s, const void *data, size_t length)
{
    struct request r = { .s = s, .in = data, .length = length };
    return call(send, &r);
}
static void receive(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    struct nx_net_tcp *t = s->pcb.tcp;
    if (s->shut_read) {   /* SHUT_RD: anything that still arrives is discarded */
        uint8_t sink[256];
        size_t n;
        while (nx_net_tcp_read(t, sink, sizeof(sink), &n) == NX_NET_OK && n) {}
        r->result = 0;
        return;
    }
    size_t done;
    if (r->peer && t->receive.cc) {   /* MSG_PEEK: copy without consuming */
        done = r->length < t->receive.cc ? r->length : t->receive.cc;
        nx_net_mbuf_copydata(t->receive.head, 0, done, r->out);
        r->result = (int64_t)done;
        return;
    }
    enum nx_net_result result = nx_net_tcp_read(t, r->out, r->length, &done);
    if (result == NX_NET_OK) r->result = (int64_t)done;
    else if (result == NX_NET_RESET || result == NX_NET_TIMED_OUT || result == NX_NET_REFUSED ||
             result == NX_NET_HOST_UNREACHABLE)
        r->result = errno_of(nx_net_tcp_take_error(t));
    else if (result == NX_NET_NOT_CONNECTED && s->connected_once) r->result = 0;   /* closed after use */
    else r->result = errno_of(result);
}
int64_t net_socket_receive(struct nx_socket *s, void *data, size_t length, bool peek)
{
    struct request r = { .s = s, .out = data, .length = length, .peer = peek };
    return call(receive, &r);
}

static void shutdown(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    if (s->type != NX_SOCK_STREAM) {   /* recorded; datagram I/O honours it in task 426 */
        s->shut_read |= r->how != NX_SHUT_WR;
        s->shut_write |= r->how != NX_SHUT_RD;
        r->result = 0;
        return;
    }
    enum nx_net_tcp_state state = s->pcb.tcp->state;
    if (state <= NX_NET_TCP_SYN_RECEIVED) { r->result = -NX_ENOTCONN; return; }
    if (r->how != NX_SHUT_WR) {   /* discard what is buffered and whatever arrives */
        s->shut_read = true;
        uint8_t sink[256];
        size_t n;
        while (nx_net_tcp_read(s->pcb.tcp, sink, sizeof(sink), &n) == NX_NET_OK && n) {}
    }
    if (r->how != NX_SHUT_RD && !s->shut_write) {
        s->shut_write = true;
        nx_net_tcp_shutdown(s->pcb.tcp);
    }
    ++s->events;
    proc_wakeup(s);
    r->result = 0;
}
int net_socket_shutdown(struct nx_socket *s, int how)
{
    struct request r = { .s = s, .how = how };
    return (int)call(shutdown, &r);
}

static void name(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    nx_net_addr_t local = 0, foreign = 0;
    nx_net_port_t local_port = 0, foreign_port = 0;
    bool connected;
    if (s->type == NX_SOCK_STREAM) {
        struct nx_net_tcp *t = s->pcb.tcp;
        local = t->local_addr; local_port = t->local_port;
        foreign = t->foreign_addr; foreign_port = t->foreign_port;
        connected = t->state >= NX_NET_TCP_ESTABLISHED;
    } else if (s->type == NX_SOCK_DGRAM) {
        local = s->pcb.udp->local_addr; local_port = s->pcb.udp->local_port;
        foreign = s->pcb.udp->foreign_addr; foreign_port = s->pcb.udp->foreign_port;
        connected = foreign != 0;
    } else {
        local = s->pcb.raw->local;
        foreign = s->pcb.raw->foreign;
        connected = foreign != 0;
    }
    if (r->peer && !connected) { r->result = -NX_ENOTCONN; return; }
    r->addr = r->peer ? foreign : local;
    r->port = r->peer ? foreign_port : local_port;
    r->result = 0;
}
int net_socket_name(struct nx_socket *s, bool peer, nx_net_addr_t *addr, nx_net_port_t *port)
{
    struct request r = { .s = s, .peer = peer };
    int64_t result = call(name, &r);
    if (!result) { *addr = r.addr; *port = r.port; }
    return (int)result;
}

/* Datagram and raw sockets (task 426). */
static void stage(void *argument)
{
    struct request *r = argument;
    struct nx_net_mbuf *m = r->chain;
    if (!m && !(m = nx_net_mbuf_get(net_pool(), true))) { r->result = -NX_ENOBUFS; return; }
    if (!nx_net_mbuf_append(m, r->in, r->length)) {
        if (!r->chain) nx_net_mbuf_freem(m);
        r->result = -NX_ENOBUFS;
        return;
    }
    r->chain = m;
    r->result = 0;
}
int net_socket_stage(struct nx_net_mbuf **chain, const void *data, size_t length)
{
    struct request r = { .chain = *chain, .in = data, .length = length };
    int64_t result = call(stage, &r);
    if (!result) *chain = r.chain;
    return (int)result;
}
static void discard(void *argument)
{
    struct request *r = argument;
    if (r->chain) nx_net_mbuf_freem(r->chain);
    r->result = 0;
}
void net_socket_discard(struct nx_net_mbuf *chain)
{
    struct request r = { .chain = chain };
    if (chain && call(discard, &r)) panic("socket chain lost its worker");
}

static void send_datagram(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    struct nx_net_mbuf *m = r->chain;
    r->chain = NULL;
    if (!m && !(m = nx_net_mbuf_get(net_pool(), true))) { r->result = -NX_ENOBUFS; return; }   /* empty datagram */
    size_t length = nx_net_mbuf_length(m);
    enum nx_net_result result;
    if (s->shut_write) { nx_net_mbuf_freem(m); r->result = -NX_EPIPE; return; }
    if (s->type == NX_SOCK_DGRAM) {
        struct nx_net_udp *u = s->pcb.udp;
        enum nx_net_result pending = nx_net_udp_take_error(u);
        if (pending != NX_NET_OK) { nx_net_mbuf_freem(m); r->result = errno_of(pending); return; }
        if (r->peer && u->foreign_addr) { nx_net_mbuf_freem(m); r->result = -NX_EISCONN; return; }
        if (!r->peer && !u->foreign_addr) { nx_net_mbuf_freem(m); r->result = -NX_EDESTADDRREQ; return; }
        if (length > NX_NET_UDP_SNDMAX || length > s->send_size) { nx_net_mbuf_freem(m); r->result = -NX_EMSGSIZE; return; }
        result = nx_net_udp_send(u, m, r->peer ? r->addr : 0, r->peer ? r->port : 0);
    } else {
        struct nx_net_raw *w = s->pcb.raw;
        if (r->peer && w->foreign) { nx_net_mbuf_freem(m); r->result = -NX_EISCONN; return; }
        if (!r->peer && !w->foreign) { nx_net_mbuf_freem(m); r->result = -NX_EDESTADDRREQ; return; }
        if (length > NX_NET_IP_MAXPACKET - 20 || length > s->send_size) { nx_net_mbuf_freem(m); r->result = -NX_EMSGSIZE; return; }
        result = nx_net_raw_send(w, m, r->peer ? r->addr : 0);
    }
    /* INVALID is a short ICMP message, else an unusable destination. */
    if (result == NX_NET_INVALID) r->result = s->type == NX_SOCK_RAW && length < 8 ? -NX_EINVAL : -NX_EADDRNOTAVAIL;
    else r->result = result == NX_NET_OK ? (int64_t)length : errno_of(result);
}
int net_socket_send_datagram(struct nx_socket *s, struct nx_net_mbuf *chain, bool addressed, nx_net_addr_t addr,
                             nx_net_port_t port)
{
    struct request r = { .s = s, .chain = chain, .peer = addressed, .addr = addr, .port = port };
    int64_t result = call(send_datagram, &r);
    if (r.chain) net_socket_discard(r.chain);   /* the worker never ran */
    return (int)result;
}

static void receive_datagram(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    r->chain = NULL;
    if (s->type == NX_SOCK_DGRAM) {
        struct nx_net_udp *u = s->pcb.udp;
        enum nx_net_result pending = nx_net_udp_take_error(u);
        if (pending != NX_NET_OK) { r->result = errno_of(pending); return; }
        if (!u->receive.head) { r->result = s->shut_read ? 0 : -NX_EAGAIN; return; }
        if (s->shut_read) { nx_net_sockbuf_flush(&u->receive); r->result = 0; return; }
        if (r->peer) {   /* peek: a copy, with the queued source prefix removed */
            struct nx_net_mbuf *copy = nx_net_mbuf_copym(u->receive.head, 0, nx_net_mbuf_length(u->receive.head));
            uint8_t meta[8];
            if (!copy) { r->result = -NX_ENOBUFS; return; }
            nx_net_mbuf_copydata(copy, 0, sizeof(meta), meta);
            nx_net_mbuf_adj_head(copy, sizeof(meta));
            r->addr = (nx_net_addr_t)meta[0] << 24 | (nx_net_addr_t)meta[1] << 16 | (nx_net_addr_t)meta[2] << 8 | meta[3];
            r->port = (nx_net_port_t)(meta[4] << 8 | meta[5]);
            r->chain = copy;
        } else {
            r->chain = nx_net_udp_receive(u, &r->addr, &r->port);
        }
    } else {
        struct nx_net_raw *w = s->pcb.raw;
        if (!w->receive.head) { r->result = s->shut_read ? 0 : -NX_EAGAIN; return; }
        if (s->shut_read) { nx_net_sockbuf_flush(&w->receive); r->result = 0; return; }
        r->chain = r->peer ? nx_net_mbuf_copym(w->receive.head, 0, nx_net_mbuf_length(w->receive.head))
                           : nx_net_raw_receive(w);
        if (!r->chain) { r->result = -NX_ENOBUFS; return; }
        uint8_t source[4];
        nx_net_mbuf_copydata(r->chain, 12, 4, source);
        r->addr = (nx_net_addr_t)source[0] << 24 | (nx_net_addr_t)source[1] << 16 | (nx_net_addr_t)source[2] << 8 | source[3];
        r->port = 0;
    }
    r->result = 0;
}
int net_socket_receive_datagram(struct nx_socket *s, bool peek, struct nx_net_mbuf **chain, nx_net_addr_t *addr,
                                nx_net_port_t *port)
{
    struct request r = { .s = s, .peer = peek };
    int64_t result = call(receive_datagram, &r);
    *chain = r.chain;
    *addr = r.addr;
    *port = r.port;
    return (int)result;
}

/* Options. Buffer sizes are clamped to [512, 262144] bytes. */
#define BUFFER_MIN 512u
#define BUFFER_MAX 262144u
static struct nx_net_sockbuf *receive_buffer(struct nx_socket *s)
{
    return s->type == NX_SOCK_STREAM ? &s->pcb.tcp->receive : s->type == NX_SOCK_DGRAM ? &s->pcb.udp->receive
                                                                                    : &s->pcb.raw->receive;
}
static void set_option(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    int32_t value = 0;
    struct nx_timeval time = {0};
    if (r->level == NX_SOL_SOCKET && (r->name == NX_SO_SNDTIMEO || r->name == NX_SO_RCVTIMEO))
        time = *(const struct nx_timeval *)r->in;
    else
        value = *(const int32_t *)r->in;
    r->result = 0;
    if (r->level == NX_IPPROTO_TCP) {
        if (s->type != NX_SOCK_STREAM) r->result = -NX_ENOPROTOOPT;
        else nx_net_tcp_set_nodelay(s->pcb.tcp, value != 0);
        return;
    }
    switch (r->name) {
    case NX_SO_REUSEADDR:
        if (s->type == NX_SOCK_STREAM) nx_net_tcp_set_reuse(s->pcb.tcp, value != 0);
        else if (s->type == NX_SOCK_DGRAM) nx_net_udp_set_reuse(s->pcb.udp, value != 0);
        else r->result = -NX_ENOPROTOOPT;   /* raw sockets have no ports */
        return;
    case NX_SO_SNDBUF: case NX_SO_RCVBUF: {
        if ((uint32_t)value > BUFFER_MAX) { r->result = -NX_ENOBUFS; return; }
        size_t size = (uint32_t)value < BUFFER_MIN ? BUFFER_MIN : (uint32_t)value;
        struct nx_net_sockbuf *b = r->name == NX_SO_RCVBUF ? receive_buffer(s)
                                 : s->type == NX_SOCK_STREAM ? &s->pcb.tcp->send : NULL;
        if (b) { b->hiwat = size; b->mbmax = 4 * size; }
        else s->send_size = (uint32_t)size;
        ++s->events;   /* room may have appeared */
        proc_wakeup(s);
        return;
    }
    case NX_SO_SNDTIMEO: case NX_SO_RCVTIMEO: {
        /* Round up to whole ticks so a nonzero timeout never means "none". */
        uint64_t ticks = (uint64_t)time.sec * NX_CLK_TCK + ((uint64_t)time.usec * NX_CLK_TCK + 999999) / 1000000;
        if (r->name == NX_SO_SNDTIMEO) s->send_timeout = ticks; else s->receive_timeout = ticks;
        return;
    }
    default: r->result = -NX_ENOPROTOOPT;
    }
}
int net_socket_set_option(struct nx_socket *s, int level, int name, const void *value)
{
    struct request r = { .s = s, .level = level, .name = name, .in = value };
    return (int)call(set_option, &r);
}
static void get_option(void *argument)
{
    struct request *r = argument;
    struct nx_socket *s = r->s;
    int32_t *value = r->out;
    r->result = 0;
    if (r->level == NX_IPPROTO_TCP) {
        if (s->type != NX_SOCK_STREAM) r->result = -NX_ENOPROTOOPT;
        else *value = s->pcb.tcp->nodelay;
        return;
    }
    switch (r->name) {
    case NX_SO_REUSEADDR:
        *value = s->type == NX_SOCK_STREAM ? s->pcb.tcp->reuse : s->type == NX_SOCK_DGRAM ? s->pcb.udp->reuse : 0;
        return;
    case NX_SO_SNDBUF:
        *value = s->type == NX_SOCK_STREAM ? (int32_t)s->pcb.tcp->send.hiwat : (int32_t)s->send_size;
        return;
    case NX_SO_RCVBUF: *value = (int32_t)receive_buffer(s)->hiwat; return;
    case NX_SO_TYPE: *value = s->type; return;
    case NX_SO_ERROR: {   /* read and clear, as in BSD */
        enum nx_net_result e = s->type == NX_SOCK_STREAM ? nx_net_tcp_take_error(s->pcb.tcp)
                             : s->type == NX_SOCK_DGRAM ? nx_net_udp_take_error(s->pcb.udp) : NX_NET_OK;
        *value = -errno_of(e);
        if (e != NX_NET_OK && s->connecting) s->connecting = false;
        return;
    }
    case NX_SO_SNDTIMEO: case NX_SO_RCVTIMEO: {
        uint64_t ticks = r->name == NX_SO_SNDTIMEO ? s->send_timeout : s->receive_timeout;
        *(struct nx_timeval *)r->out = (struct nx_timeval){ (int64_t)(ticks / NX_CLK_TCK),
                                                            (int64_t)(ticks % NX_CLK_TCK * (1000000 / NX_CLK_TCK)) };
        return;
    }
    default: r->result = -NX_ENOPROTOOPT;
    }
}
int net_socket_get_option(struct nx_socket *s, int level, int name, void *value)
{
    struct request r = { .s = s, .level = level, .name = name, .out = value };
    return (int)call(get_option, &r);
}
