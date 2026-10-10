#ifndef NEXIX_LIBC_UNISTD_H
#define NEXIX_LIBC_UNISTD_H
#include <sys/types.h>
#include <stddef.h>
#include <nexix/file.h>
#define SEEK_SET NX_SEEK_SET
#define SEEK_CUR NX_SEEK_CUR
#define SEEK_END NX_SEEK_END
#define PIPE_BUF NX_PIPE_BUF
ssize_t read(int fd, void *buffer, size_t count);
ssize_t write(int fd, const void *buffer, size_t count);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int dup(int fd);
int dup2(int fd, int target);
int pipe(int pair[2]);
int chdir(const char *path);
int link(const char *oldpath, const char *newpath);
int unlink(const char *path);
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
