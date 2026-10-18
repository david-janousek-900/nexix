#ifndef NEXIX_TTY_CONTROLLER_H
#define NEXIX_TTY_CONTROLLER_H
#include <nexix/tty.h>
#include <stdbool.h>
struct tty_parameters {unsigned rate;uint8_t format;bool software_parity;};
int tty_controller_device(uint16_t,unsigned *profile,unsigned *line);
int tty_parameters(unsigned,const struct nx_sgttyb *,struct tty_parameters *);
bool tty_controller_input(unsigned,uint32_t,uint8_t,unsigned,uint8_t *);
uint8_t tty_character(uint8_t); /* V7 partab classification and even parity. */
uint8_t tty_mapping(uint8_t); /* V7 canonical escape map. */
unsigned tty_delay(unsigned profile,unsigned ticks); /* V7 60 Hz -> native 100 Hz. */
#endif
