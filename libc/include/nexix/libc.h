#ifndef NEXIX_LIBC_NATIVE_H
#define NEXIX_LIBC_NATIVE_H
/* Raw ABI: signed result/negative errno, does not set libc errno. */
long nx_syscall6(long number, long a1, long a2, long a3, long a4, long a5, long a6);
#endif
