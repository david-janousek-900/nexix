#ifndef NEXIX_NET_COMPAT_H
#define NEXIX_NET_COMPAT_H
#include "net/types.h"

/* Fresh native implementations for the BSD port. No global u_long, BSD errno,
 * system-header replacement, allocator or interrupt-level stubs are provided.
 */

/* Circular sequence ordering is defined only within a half-range window.
 * Exactly 2^31 apart is unordered; equality is still handled by le/ge.
 * Unsigned subtraction wraps intentionally, without signed overflow/casts.
 */
static inline bool nx_net_seq_lt(nx_net_seq_t a, nx_net_seq_t b)
{ return (uint32_t)(b - a) != 0 && (uint32_t)(b - a) < UINT32_C(0x80000000); }
static inline bool nx_net_seq_le(nx_net_seq_t a, nx_net_seq_t b)
{ return a == b || nx_net_seq_lt(a, b); }
static inline bool nx_net_seq_gt(nx_net_seq_t a, nx_net_seq_t b)
{ return nx_net_seq_lt(b, a); }
static inline bool nx_net_seq_ge(nx_net_seq_t a, nx_net_seq_t b)
{ return a == b || nx_net_seq_gt(a, b); }

/* Failure leaves *result unchanged. result must be a valid pointer. */
bool nx_net_size_add(size_t a, size_t b, size_t *result);
bool nx_net_size_mul(size_t a, size_t b, size_t *result);
bool nx_net_size_align(size_t value, size_t alignment, size_t *result);

/* One's-complement Internet checksum, independent of host alignment/order.
 * Keep state across fragments: an odd final byte of one chunk pairs with the
 * first byte of the next. finish does not consume state and returns a host-order
 * value to store with nx_net_put16. A verified packet including its checksum
 * finishes as zero. Protocol-specific UDP zero-to-ffff handling is the caller's.
 */
struct nx_net_checksum { uint32_t sum; uint8_t pending; bool odd; };
void nx_net_checksum_init(struct nx_net_checksum *);
/* data may be NULL only when length is zero. */
void nx_net_checksum_update(struct nx_net_checksum *, const void *data, size_t length);
uint16_t nx_net_checksum_finish(const struct nx_net_checksum *);
/* Append an IPv4 transport pseudoheader, with host-order arguments. */
void nx_net_checksum_pseudo(struct nx_net_checksum *, nx_net_addr_t src,
                            nx_net_addr_t dst, uint8_t protocol, uint16_t length);
#endif
