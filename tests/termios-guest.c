#include <termios.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
static volatile int interrupted;
static void caught(int n){(void)n;interrupted=1;}
int main(void)
{
 struct termios original,visual,after;struct nx_sgttyb sg,restored;
 struct winsize size;char c;
 if(tcgetattr(0,&original) || ioctl(0,TIOCGETP,&sg))return 1;
 if(ioctl(0,TIOCGWINSZ,&size) || size.ws_row!=24 || size.ws_col!=80)return 2;
 int fd=open("/dev/tty0",O_RDWR);
 if(fd<0 || ioctl(fd,TIOCGWINSZ,&size) || size.ws_row!=25 || size.ws_col!=80)return 3;
 close(fd);
 visual=original;visual.c_lflag&=~(ECHO|ICANON);visual.c_iflag&=~ICRNL;visual.c_oflag&=~ONLCR;
 visual.c_cc[VMIN]=0;
 if(tcsetattr(0,TCSANOW,&visual)!=-1 || errno!=EINVAL)return 4;
 if(tcgetattr(0,&after) || memcmp(&after,&original,sizeof(after)))return 5;
 visual.c_cc[VMIN]=1;visual.c_cc[VTIME]=1;
 if(signal(SIGINT,caught)==SIG_ERR || tcsetattr(0,TCSADRAIN,&visual))return 6;
 write(1,"TERMIOS_READY\r\n",15);
 int n=read(0,&c,1),saved=errno;
 if(tcsetattr(0,TCSAFLUSH,&original))return 7;
 if(ioctl(0,TIOCGETP,&restored) || memcmp(&sg,&restored,sizeof(sg)))return 8;
 if(n!=-1 || saved!=EINTR || !interrupted)return 9;
 fd=open("/etc/passwd",O_RDONLY);
 if(fd<0 || tcgetattr(fd,&after)!=-1 || errno!=ENOTTY)return 10;
 close(fd);
 puts("TERMIOS_OK");return 0;
}
