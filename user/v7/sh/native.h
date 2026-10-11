#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <signal.h>
#include <sgtty.h>
/* Shell's fault function historically returns an unused int. */
#define signal(n,h) signal((n),(sighandler_t)(h))
extern void exit(int);
