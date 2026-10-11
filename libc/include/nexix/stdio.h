#ifndef NEXIX_LIBC_STDIO_ADAPTER_H
#define NEXIX_LIBC_STDIO_ADAPTER_H
#include <stdarg.h>
/* Native replacement for V7 %r's pointer to a format/argument stack block.
 * Initialize/end in the variadic caller's lifetime; do not copy with memcpy. */
struct nx_printf_remote { const char *format; va_list arguments; };
void nx_printf_remote_init(struct nx_printf_remote *, const char *, va_list);
void nx_printf_remote_end(struct nx_printf_remote *);
#endif
