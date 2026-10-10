#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define EXIT_HANDLERS 32
static void (*handlers[EXIT_HANDLERS])(void);
static unsigned handler_count;
int atexit(void (*function)(void))
{
    if (!function || handler_count == EXIT_HANDLERS) return -1;
    handlers[handler_count++] = function;
    return 0;
}
_Noreturn void exit(int status)
{
    while (handler_count) handlers[--handler_count]();
    _exit(status);
}
char *getenv(const char *name)
{
    if (!*name || strchr(name, '=') || !environ) return NULL;
    size_t n = strlen(name);
    for (char **entry = environ; *entry; ++entry)
        if (!strncmp(*entry, name, n) && (*entry)[n] == '=') return *entry + n + 1;
    return NULL;
}
