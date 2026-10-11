#ifndef NEXIX_LIBC_STDLIB_H
#define NEXIX_LIBC_STDLIB_H
#include <stddef.h>
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
/* V7 abort may return if SIGIOT is caught or ignored. */
int abort(void);
void ltol3(char *, const long *, int);
void l3tol(long *, const char *, int);
char *gcvt(double, int, char *);
_Noreturn void exit(int status);
int atexit(void (*function)(void));
void *malloc(size_t);
void free(void *);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
int atoi(const char *);
long atol(const char *);
long strtol(const char *,char **,int);
unsigned long strtoul(const char *,char **,int);
int abs(int);
long labs(long);
#define RAND_MAX 32767
int rand(void);
void srand(unsigned);
double atof(const char *);
double strtod(const char *, char **);
char *ecvt(double, int, int *, int *);
char *fcvt(double, int, int *, int *);
char *getenv(const char *name);
#endif
