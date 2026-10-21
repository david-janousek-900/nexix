#include "net/compat.h"
#include "net/wire.h"

bool nx_net_size_add(size_t a, size_t b, size_t *result)
{
    if (a > SIZE_MAX - b) return false;
    *result = a + b;
    return true;
}
bool nx_net_size_mul(size_t a, size_t b, size_t *result)
{
    if (b && a > SIZE_MAX / b) return false;
    *result = a * b;
    return true;
}
bool nx_net_size_align(size_t value, size_t alignment, size_t *result)
{
    size_t rounded;
    if (!alignment || (alignment & (alignment - 1)) ||
        !nx_net_size_add(value, alignment - 1, &rounded)) return false;
    *result = rounded & ~(alignment - 1);
    return true;
}

void nx_net_checksum_init(struct nx_net_checksum *state)
{
    state->sum = 0; state->pending = 0; state->odd = false;
}
static uint32_t fold(uint32_t sum)
{ return (sum & UINT32_C(0xffff)) + (sum >> 16); }
void nx_net_checksum_update(struct nx_net_checksum *state, const void *data, size_t length)
{
    const uint8_t *p = data;
    if (!length) return;
    if (state->odd) {
        state->sum = fold(state->sum + ((uint32_t)state->pending << 8) + *p++);
        --length; state->odd = false;
    }
    while (length >= 2) {
        state->sum = fold(state->sum + nx_net_get16(p));
        p += 2; length -= 2;
    }
    if (length) { state->pending = *p; state->odd = true; }
}
uint16_t nx_net_checksum_finish(const struct nx_net_checksum *state)
{
    uint32_t sum = state->sum;
    if (state->odd) sum += (uint32_t)state->pending << 8;
    sum = fold(fold(sum));
    return (uint16_t)~sum;
}
void nx_net_checksum_pseudo(struct nx_net_checksum *state, nx_net_addr_t src,
                            nx_net_addr_t dst, uint8_t protocol, uint16_t length)
{
    struct nx_net_pseudo_wire header;
    nx_net_put32(header.src, src); nx_net_put32(header.dst, dst);
    header.zero = 0; header.protocol = protocol;
    nx_net_put16(header.length, length);
    nx_net_checksum_update(state, &header, sizeof(header));
}
