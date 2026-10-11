/* Native source adaptation of V7 usr/src/libc/gen/mktemp.c. */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
char *mktemp(char *name)
{
    /* Bound the historical backward scan, including empty/all-X strings. */
    if (!name) { errno=EINVAL; return "/"; }
    char *end=name+strlen(name), *suffix=end;
    if (end==name) { errno=EINVAL; return "/"; }
    while (suffix>name && suffix[-1]=='X') --suffix;
    if (suffix==end) {
        /* V7 returns an unused name unchanged even without a trailing X.
         * An occupied such name has no writable replacement character. */
        if (access(name,F_OK)==-1) return name;
        errno=EINVAL; return "/";
    }
    unsigned pid=(unsigned)getpid();
    for (char *p=end; p>suffix;) { *--p='0'+pid%10; pid/=10; }
    char letter='a';
    while (access(name,F_OK)!=-1) {
        /* Historical exhaustion sentinel; V7 never tries letter z. */
        if (letter=='z') return "/";
        *suffix=letter++;
    }
    return name;
}
