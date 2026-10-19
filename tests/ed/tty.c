#include <errno.h>
#include "sgtty.h"
int gtty(int fd,struct sgttyb *s){(void)fd;(void)s;errno=ENOTTY;return -1;}
int stty(int fd,const struct sgttyb *s){(void)fd;(void)s;errno=ENOTTY;return -1;}
