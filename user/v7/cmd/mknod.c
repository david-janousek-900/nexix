/* Native adaptation of reference/v7/usr/src/cmd/mknod.c. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int number(const char *s)
{
    unsigned n = 0;
    if (!*s) return -1;
    while (*s) {
        if (*s < '0' || *s > '9') return -1;
        n = n * 10 + (unsigned)(*s++ - '0');
        if (n > 255) return -1;
    }
    return (int)n;
}

int main(int argc, char **argv)
{
    int a, b;
    mode_t mode;
    if (argc != 5) goto usage;
    if (!strcmp(argv[2], "b")) mode = S_IFBLK | 0666;
    else if (!strcmp(argv[2], "c")) mode = S_IFCHR | 0666;
    else goto usage;
    a = number(argv[3]); b = number(argv[4]);
    if (a < 0 || b < 0) goto usage;
    if (mknod(argv[1], mode, makedev(a, b)) < 0) {
        perror("mknod");
        return 1;
    }
    return 0;
usage:
    fprintf(stderr, "usage: mknod name b/c major minor\n");
    return 2;
}
