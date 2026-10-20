#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
 int fd=open("/dev/tty0",O_WRONLY);if(fd<0)return 1;
 const char *parts[]={"old text\033[H\033[2J\033[3;","4H\033[7mZ\033[0mQ"};
 for(unsigned i=0;i<2;++i)if(write(fd,parts[i],strlen(parts[i]))!=(int)strlen(parts[i]))return 2;
 if(close(fd))return 3;
 puts("VGA_CONTROLS_OK");return 0;
}
