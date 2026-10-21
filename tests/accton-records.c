/* Inspect records produced by installed commands without enabling accounting. */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/acct.h>
#include <string.h>

int main(int argc, char **argv)
{
    struct acct record;
    struct stat st;
    int fd, found = 0;
    if (argc != 2 || (fd = open(argv[1], O_RDONLY)) < 0) return 1;
    if (fstat(fd, &st) || !st.st_size || st.st_size % sizeof(record)) return 2;
    for (;;) {
        ssize_t n = read(fd, &record, sizeof(record));
        if (!n) break;
        if (n != sizeof(record)) return 3;
        for (unsigned i = 0; i < sizeof(record.ac_reserved); ++i)
            if (record.ac_reserved[i]) return 4;
        if (!memcmp(record.ac_comm, "sum", 3) && record.ac_uid == 101) found = 1;
    }
    close(fd);
    if (!found) return 5;
    puts("ACCTON_RECORDS_OK");
    return 0;
}
