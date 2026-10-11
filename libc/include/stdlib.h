#ifndef NEXIX_LIBC_STDLIB_H
#define NEXIX_LIBC_STDLIB_H
#include <stddef.h>
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
_Noreturn void exit(int status);
int atexit(void (*function)(void));
void *malloc(size_t);
void free(void *);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
int atoi(const char *);
double atof(const char *);
double strtod(const char *, char **);
char *ecvt(double, int, int *, int *);
char *fcvt(double, int, int *, int *);
char *getenv(const char *name);
#endif
