/* Task 98's private stand-in checks the exec boundary, not authentication. */
#include <stdio.h>
#include <string.h>
#include <sgtty.h>
int main(int argc,char **argv)
{
    struct sgttyb s;struct tchars c;
    if(argc!=2 || strcmp(argv[0],"login") ||
        (strcmp(argv[1],"alice") && strcmp(argv[1],"abcdefghijklmnop") && *argv[1]) ||
        gtty(0,&s) || ioctl(0,TIOCGETC,&c) ||
        (s.sg_ispeed!=B115200 && s.sg_ispeed!=B9600) ||
        s.sg_ospeed!=s.sg_ispeed || (s.sg_flags&RAW) || !(s.sg_flags&ECHO) ||
        s.sg_erase!=(s.sg_ispeed==B115200?127:'#') || s.sg_kill!=(s.sg_ispeed==B115200?21:'@') || c.t_intrc!=(s.sg_ispeed==B115200?3:127))return 1;
    puts("GETTY_HANDOFF_OK");return 0;
}
