/* Native adaptation of reference/v7/usr/src/cmd/update.c. */
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

static void dosync(int sig)
{
    (void)sig;
    sync();
    signal(SIGALRM, dosync);
    alarm(30);
}

int main(void)
{
    static const char *const dirs[] = {"/bin", "/usr", "/usr/bin", NULL};
    unsigned i;
    pid_t pid = fork();
    if (pid < 0) { perror("update"); return 1; }
    if (pid) return 0;
    close(0); close(1); close(2);
    for (i = 0; dirs[i]; ++i) (void)open(dirs[i], O_RDONLY);
    dosync(0);
    for (;;) pause();
}
