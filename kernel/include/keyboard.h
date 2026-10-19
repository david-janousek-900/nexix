#ifndef NEXIX_KEYBOARD_H
#define NEXIX_KEYBOARD_H
#include <stdbool.h>
#include <stdint.h>
/* Call with interrupts disabled. Initialization failure leaves IRQ1 masked. */
bool keyboard_init(void (*receive)(uint8_t));
void keyboard_interrupt(void);
/* Translated set-1 scan codes; exposed for deterministic decoder tests. */
void keyboard_decode(uint8_t code);
#endif
