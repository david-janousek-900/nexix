#ifndef NEXIX_PACKET_H
#define NEXIX_PACKET_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <nexix/packet.h>
#define PK_CLOSE 1
#define PK_RJ 2
#define PK_SRJ 3
#define PK_RR 4
#define PK_INITC 5
#define PK_INITB 6
#define PK_INITA 7
#define PK_RETRY 300U
#define PK_RETRIES 10U
struct pk_buffer { uint8_t data[NX_PK_MAX_SIZE]; uint16_t length,offset; uint8_t sequence; bool sent; };
struct packet {
    bool (*emit)(void *,const uint8_t *,size_t);
    void (*notice)(void *);
    void *context;
    uint64_t deadline,last_byte;
    unsigned state,rsize,xsize,rwindow,swindow,rx_head,rx_count,tx_head,tx_count;
    unsigned rx_ack,rx_accept,tx_sequence,tx_ack,pending,retries;
    unsigned header_count,body_count,body_size;
    bool init_a,init_b,remote_closed,eof_reported,bad;
    uint8_t header[6],body[NX_PK_MAX_SIZE];
    struct pk_buffer rx[NX_PK_WINDOW],tx[NX_PK_WINDOW];
};
uint16_t pk_checksum(const uint8_t *,unsigned);
bool pk_size_valid(unsigned);
void pk_initialize(struct packet *,unsigned,unsigned,bool (*)(void *,const uint8_t *,size_t),void (*)(void *),void *,uint64_t);
void pk_receive(struct packet *,uint8_t,uint64_t);
void pk_tick(struct packet *,uint64_t);
bool pk_readable(const struct packet *);
size_t pk_take(struct packet *,uint8_t *,size_t,bool *);
int pk_send(struct packet *,const uint8_t *,size_t,uint64_t); /* 0 success, EAGAIN window full */
void pk_shutdown(struct packet *);
/* Kernel ownership adapter. All entry points require IRQ serialization. */
struct tty;struct address_space;
bool packet_active(struct tty *);
int packet_open(struct tty *,struct nx_piocb *);
void packet_close(struct tty *);
int packet_status(struct tty *,struct nx_piocb *);
void packet_receive(struct tty *,uint8_t);
void packet_changed(struct tty *);
void packet_timer(uint64_t);
int64_t packet_io(struct tty *,struct address_space *,uintptr_t,size_t,bool);
bool packet_ready(struct tty *);
size_t packet_take(struct tty *,uint8_t *,size_t,bool *);
int64_t packet_put(struct tty *,const uint8_t *,size_t);
bool tty_packet_emit(struct tty *,const uint8_t *,size_t);
void tty_packet_notice(struct tty *);
bool tty_packet_reader(struct tty *);
void mpx_tty_discipline(struct tty *);
#endif
