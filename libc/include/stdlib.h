#ifndef NEXIX_LIBC_STDLIB_H
#define NEXIX_LIBC_STDLIB_H
#include <stddef.h>
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
_Noreturn void exit(int status);
int atexit(void (*function)(void));
char *getenv(const char *name);
#endif
