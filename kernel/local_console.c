#include "console.h"
/* Independent local terminal: synchronous VGA output and IRQ-driven PS/2 input. */
#include "local_console.h"
#include "vga.h"
#include "keyboard.h"
#include "tty.h"
#include "serial.h"
#include <nexix/modem.h>
static bool enqueue(void *v,const uint8_t *bytes,size_t n)
{(void)v;vga_write(bytes,n);return true;}
static bool idle(void *v){(void)v;return true;}
static void flush(void *v){(void)v;}
static void format(void *v,unsigned rate,uint8_t bits){(void)v;(void)rate;(void)bits;}
static void outputs(void *v,bool dtr,bool rts){(void)v;(void)dtr;(void)rts;}
static unsigned status(void *v){(void)v;return NX_MDM_DCD|NX_MDM_CTS|NX_MDM_DSR;}
static const struct tty_driver driver={NULL,enqueue,idle,flush,format,outputs,status};
static void receive(uint8_t c){tty_receive_line(2,c,0);}
void local_console_init(void)
{
    if(tty_attach(2,&driver))return;
    if(!keyboard_init(receive)){
        static const uint8_t message[]="Nexix: PS/2 keyboard unavailable\r\n";
        console_puts((const char *)message);
    }
}
