#ifndef NEXIX_LIBC_UNISTD_H
#define NEXIX_LIBC_UNISTD_H
#include <sys/types.h>
#include <crypt.h>
#include <sys/phys.h>
int mknod(const char *, mode_t, dev_t);
int chroot(const char *);
int mount(const char *device, const char *mountpoint, int read_only);
int umount(const char *device);
int acct(const char *file);
int lock(int flag);
int profil(unsigned short *buffer,size_t size,uintptr_t offset,unsigned int scale);
#include <stddef.h>
#include <nexix/file.h>
#define SEEK_SET NX_SEEK_SET
#define SEEK_CUR NX_SEEK_CUR
#define SEEK_END NX_SEEK_END
#define PIPE_BUF NX_PIPE_BUF
ssize_t read(int fd, void *buffer, size_t count);
ssize_t write(int fd, const void *buffer, size_t count);
int close(int fd);
void swab(const void *, void *, ssize_t);
off_t tell(int fd);
off_t lseek(int fd, off_t offset, int whence);
int dup(int fd);
int dup2(int fd, int target);
int pipe(int pair[2]);
int chdir(const char *path);
int link(const char *oldpath, const char *newpath);
int unlink(const char *path);
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
int access(const char *, int);
int setuid(uid_t);
int setgid(gid_t);
int nice(int);
pid_t getpgrp(void);
pid_t setpgrp(void);
unsigned alarm(unsigned);
unsigned sleep(unsigned);
int pause(void);
int brk(void *);
void *sbrk(intptr_t);
void sync(void);
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
int isatty(int fd);
char *getpass(const char *prompt);
char *ttyname(int fd);
int ttyslot(void);
char *getlogin(void);
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
int execle(const char *, const char *, ...);
int execl(const char *, const char *, ...);
int execvp(const char *, char *const []);
int execv(const char *path, char *const argv[]);
#endif
