#ifndef NEXIX_LIBC_SIGNAL_H
#define NEXIX_LIBC_SIGNAL_H
#include <sys/types.h>
#include <sys/ptrace.h>
#include <nexix/signal.h>
#define SIGHUP NX_SIGHUP
#define SIGINT NX_SIGINT
#define SIGQUIT NX_SIGQUIT
#define SIGILL NX_SIGILL
#define SIGTRAP NX_SIGTRAP
#define SIGABRT NX_SIGABRT
#define SIGIOT NX_SIGABRT
#define SIGEMT NX_SIGEMT
#define SIGFPE NX_SIGFPE
#define SIGKILL NX_SIGKILL
#define SIGBUS NX_SIGBUS
#define SIGSEGV NX_SIGSEGV
#define SIGSYS NX_SIGSYS
#define SIGPIPE NX_SIGPIPE
#define SIGALRM NX_SIGALRM
#define SIGTERM NX_SIGTERM
/* Historical spellings used by original kernel-facing consumers. */
#define SIGINS SIGILL
#define SIGTRC SIGTRAP
#define SIGFPT SIGFPE
#define SIGKIL SIGKILL
#define SIGSEG SIGSEGV
#define SIGCLK SIGALRM
#define SIGTRM SIGTERM
#define NSIG NX_NSIG
typedef int sig_atomic_t;
typedef void (*sighandler_t)(int);
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)
sighandler_t signal(int, sighandler_t);
int kill(pid_t, int);
int raise(int);
#endif
