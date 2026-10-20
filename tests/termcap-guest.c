#include <termcap.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
extern char **environ;
int main(void)
{
 char buffer[TCBUFSIZE],strings[TCBUFSIZE],*area=strings;
 char *env[]={"TERMCAP=/tmp/termcap",0};environ=env;
 const char *text="base:co#80:li#24:am:cl=\\E[H\\E[2J:\nchild:li#25:tc=base:\n";
 int fd=creat("/tmp/termcap",0600);
 if(fd<0 || write(fd,text,strlen(text))!=(int)strlen(text) || close(fd))return 1;
 if(tgetent(buffer,"child")!=1 || tgetnum("li")!=25 || tgetnum("co")!=80 || !tgetflag("am") || strcmp(tgetstr("cl",&area),"\033[H\033[2J"))return 2;
 if(tgetent(buffer,"absent") || unlink("/tmp/termcap"))return 3;
 puts("TERMCAP_OK");return 0;
}
