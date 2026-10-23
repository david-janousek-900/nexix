/*
 * Host-test shim linked with --wrap: directories opened with open() read
 * as V7 16-byte entries, and stat() reports that size, as native Nexix
 * directories do. Only the host fixtures use it.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int __real_open(const char *, int, ...);
ssize_t __real_read(int, void *, size_t);
off_t __real_lseek(int, off_t, int);
int __real_close(int);
int __real_stat(const char *, struct stat *);

static struct { char *buf; size_t len, pos; } table[1024];

static char *listing(const char *path, size_t *len)
{
    DIR *d = opendir(path);
    struct dirent *e;
    char *buf = NULL;
    size_t n = 0;
    if (!d) return NULL;
    while ((e = readdir(d)) != NULL) {
        unsigned char record[16] = {0};
        uint16_t ino = (uint16_t)e->d_ino ? (uint16_t)e->d_ino : 1;
        memcpy(record, &ino, 2);
        strncpy((char *)record + 2, e->d_name, 14);
        buf = realloc(buf, n + 16);
        memcpy(buf + n, record, 16);
        n += 16;
    }
    closedir(d);
    *len = n;
    return buf ? buf : malloc(1);
}

int __wrap_open(const char *path, int flags, ...)
{
    struct stat st;
    mode_t mode = 0;
    va_list ap;
    va_start(ap, flags);
    if (flags & O_CREAT) mode = va_arg(ap, int);
    va_end(ap);
    if (__real_stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        size_t len;
        char *buf = listing(path, &len);
        int fd;
        if (!buf) return -1;
        fd = __real_open("/dev/null", O_RDONLY);
        if (fd < 0 || fd >= 1024) { free(buf); errno = EMFILE; return -1; }
        table[fd].buf = buf; table[fd].len = len; table[fd].pos = 0;
        return fd;
    }
    return __real_open(path, flags, mode);
}

ssize_t __wrap_read(int fd, void *out, size_t n)
{
    if (fd >= 0 && fd < 1024 && table[fd].buf) {
        size_t left = table[fd].len - table[fd].pos;
        if (n > left) n = left;
        memcpy(out, table[fd].buf + table[fd].pos, n);
        table[fd].pos += n;
        return n;
    }
    return __real_read(fd, out, n);
}

off_t __wrap_lseek(int fd, off_t offset, int whence)
{
    if (fd >= 0 && fd < 1024 && table[fd].buf) {
        off_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (off_t)table[fd].pos : (off_t)table[fd].len;
        if (base + offset < 0) { errno = EINVAL; return -1; }
        table[fd].pos = base + offset;
        return table[fd].pos;
    }
    return __real_lseek(fd, offset, whence);
}

int __wrap_close(int fd)
{
    if (fd >= 0 && fd < 1024 && table[fd].buf) { free(table[fd].buf); table[fd].buf = NULL; }
    return __real_close(fd);
}

int __wrap_stat(const char *path, struct stat *st)
{
    int r = __real_stat(path, st);
    if (r == 0 && S_ISDIR(st->st_mode)) {
        size_t len;
        char *buf = listing(path, &len);
        if (buf) { st->st_size = len; free(buf); }
    }
    return r;
}
