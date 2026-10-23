/* Run a command as another user: as-user uid program [arguments]. */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
int main(int argc, char **argv)
{
    if (argc < 3 || setgid((gid_t)atoi(argv[1])) < 0 || setuid((uid_t)atoi(argv[1])) < 0) return 125;
    execv(argv[2], argv + 2);
    perror(argv[2]);
    return 126;
}
