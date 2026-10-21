/* V7 df free-stack traversal with explicit PDP-11 disk decoding. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

static unsigned short16(const unsigned char *p)
{ return p[0] | ((unsigned)p[1] << 8); }
static uint32_t long32(const unsigned char *p)
{ return ((uint32_t)short16(p) << 16) | short16(p + 2); }

static int bread(int fd, uint32_t block, unsigned char *data)
{
    if (lseek(fd, (off_t)block * 512, SEEK_SET) < 0 || read(fd, data, 512) != 512) {
        fprintf(stderr, "df: read error %lu\n", (unsigned long)block);
        return -1;
    }
    return 0;
}

static int dfree(const char *file)
{
    unsigned char super[512], chain[512], *seen = NULL;
    uint32_t stack[50], blocks, first, block, count = 0;
    unsigned n, i;
    int fd = open(file, O_RDONLY), result = 1;
    if (fd < 0) { fprintf(stderr, "cannot open %s\n", file); return 1; }
    sync();
    if (bread(fd, 1, super)) goto out;
    first = short16(super); blocks = long32(super + 2); n = short16(super + 6);
    if (first < 3 || blocks <= first || blocks > (1U << 24)) goto corrupt;
    seen = calloc(((size_t)blocks + 7) / 8, 1);
    if (!seen) { fprintf(stderr, "df: out of memory\n"); goto out; }
    for (i = 0; i < 50; ++i) stack[i] = long32(super + 8 + 4 * i);
    for (;;) {
        if (!n || n > 50) goto corrupt;
        block = stack[--n];
        if (!block) { if (n) goto corrupt; break; }
        if (block < first || block >= blocks || (seen[block / 8] & (1U << (block % 8))))
            goto corrupt;
        seen[block / 8] |= 1U << (block % 8);
        ++count;
        if (!n) {
            if (bread(fd, block, chain)) goto out;
            n = short16(chain);
            for (i = 0; i < 50; ++i) stack[i] = long32(chain + 2 + 4 * i);
        }
    }
    printf("%s %lu\n", file, (unsigned long)count);
    result = fflush(stdout) == EOF;
    goto out;
corrupt:
    fprintf(stderr, "df: corrupt free list on %s\n", file);
out:
    free(seen); close(fd); return result;
}

int main(int argc, char **argv)
{
    int i, result = 0;
    if (argc == 1) return dfree("/dev/root");
    for (i = 1; i < argc; ++i) result |= dfree(argv[i]);
    return result;
}
