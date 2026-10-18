/* Legacy cooked/raw TTY fixtures have no attached packet discipline.
 * The real packet core/adapter are exercised by packet-unit and packet QEMU. */
#include "packet.h"
#include "uaccess.h"
#include "serial.h"
#include <nexix/errno.h>
bool packet_active(struct tty *t){(void)t;return false;}
int packet_open(struct tty *t,struct nx_piocb *p){(void)t;(void)p;return -NX_ENXIO;}
void packet_changed(struct tty *t){(void)t;}
void packet_close(struct tty *t){(void)t;}
int packet_status(struct tty *t,struct nx_piocb *p){(void)t;(void)p;return -NX_ENXIO;}
void packet_receive(struct tty *t,uint8_t b){(void)t;(void)b;}
int64_t packet_io(struct tty *t,struct address_space *v,uintptr_t a,size_t n,bool w)
{(void)t;(void)v;(void)a;(void)n;(void)w;return -NX_ENXIO;}
bool packet_ready(struct tty *t){(void)t;return false;}
size_t packet_take(struct tty *t,uint8_t *b,size_t n,bool *e){(void)t;(void)b;(void)n;*e=false;return 0;}
int64_t packet_put(struct tty *t,const uint8_t *b,size_t n){(void)t;(void)b;(void)n;return -NX_ENXIO;}
__attribute__((weak)) void mpx_tty_discipline(struct tty *t){(void)t;}
__attribute__((weak)) bool serial_enqueue(const uint8_t *b,size_t n){while(n--)serial_putc(*b++);return true;}
__attribute__((weak)) bool user_access_ok(const struct address_space *v,uintptr_t a,size_t n,bool w)
{(void)v;(void)n;(void)w;return a>=4096;}

__attribute__((weak)) uint64_t timer_ticks(void){return 0;}
__attribute__((weak)) void serial_format(unsigned rate,uint8_t control)
{serial_configure(rate,(control&8)?((control&16)?2:1):0);}
__attribute__((weak)) void serial_flush(void){}
__attribute__((weak)) void serial_dtr(bool on){(void)on;}

__attribute__((weak)) bool serial_idle(void){return true;}
