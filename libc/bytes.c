/* Native adapters for V7 gen/swab.c and gen/l3.c. */
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
void swab(const void *source, void *destination, ssize_t count)
{
    const unsigned char *from = source;
    unsigned char *to = destination;
    /* As in V7, an odd trailing byte is untouched. Exact in-place operation
     * and unaligned byte addresses work; other overlap is not promised. */
    while (count >= 2) {
        unsigned char first = from[0], second = from[1];
        to[0] = second; to[1] = first;
        from += 2; to += 2; count -= 2;
    }
}
void ltol3(char *packed, const long *values, int count)
{
    unsigned char *bytes = (unsigned char *)packed;
    for (int i = 0; i < count; ++i) {
        unsigned long value = (unsigned long)values[i];
        /* PDP-11 long words are high-word first, little endian within words.
         * V7 ltol3 discards all bits above the low 24 (including sign bits). */
        bytes[0] = value >> 16; bytes[1] = value; bytes[2] = value >> 8;
        bytes += 3;
    }
}
void l3tol(long *values, const char *packed, int count)
{
    const unsigned char *bytes = (const unsigned char *)packed;
    for (int i = 0; i < count; ++i) {
        values[i] = (long)((uint32_t)bytes[0] << 16 | (uint32_t)bytes[2] << 8 | bytes[1]);
        bytes += 3;
    }
}
