#ifndef NEXIX_LIBC_UNISTD_H
#define NEXIX_LIBC_UNISTD_H
#include <sys/types.h>
extern char **environ;
_Noreturn void _exit(int status);
pid_t fork(void);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int execve(const char *path, char *const argv[], char *const envp[]);
int execv(const char *path, char *const argv[]);
#endif
