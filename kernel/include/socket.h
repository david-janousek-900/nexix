#ifndef NEXIX_SOCKET_H
#define NEXIX_SOCKET_H
#include <stdint.h>
/* Native socket syscalls 77-88 (task 424): argument validation at the user
 * boundary, documented in docs/sockets.md. */
int64_t socket_syscall(uint64_t number, const uintptr_t args[6]);
#endif
