#ifndef NEXIX_SOCKET_H
#define NEXIX_SOCKET_H
#include <stdbool.h>
#include <stdint.h>
/* Native socket syscalls 77-88 (task 424): argument validation at the user
 * boundary and descriptors, documented in docs/sockets.md. */
int64_t socket_syscall(uint64_t number, const uintptr_t args[6]);
/* Descriptor hooks (task 425): read/write of a validated user range on a
 * socket file, and its final close. */
int64_t socket_file_io(void *socket, uint32_t open_flags, uintptr_t address, uint64_t count, bool write);
void socket_file_close(void *socket);
/* NX_SIOC* configuration requests on a socket descriptor (task 427). */
int64_t socket_ioctl(void *socket, uint64_t request, uintptr_t argument);
#endif
