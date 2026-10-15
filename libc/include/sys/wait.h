#ifndef NEXIX_LIBC_SYS_WAIT_H
#define NEXIX_LIBC_SYS_WAIT_H
#include <sys/types.h>
pid_t wait(int *status);
#define WIFEXITED(s) (((s) & 255) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 255)
#define WIFSIGNALED(s) (((s) & 127) != 0 && ((s) & 127) != 127)
#define WTERMSIG(s) ((s) & 127)
#define WCOREDUMP(s) (((s) & 0200) != 0)
#define WIFSTOPPED(s) (((s) & 255) == 0177)
#define WSTOPSIG(s) (((s) >> 8) & 255)
#endif
