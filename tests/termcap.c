#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "../libc/include/termcap.h"
int main(void)
{
 char path[]="/tmp/nx-termcap-XXXXXX",buffer[TCBUFSIZE],strings[TCBUFSIZE],*area=strings;
 int fd=mkstemp(path);assert(fd>=0);
 const char *data="base|alias:co#80:li#030:am:cl=\\E[H\\E[2J:\nchild:li#25:am@:tc=base:\ncycle:tc=cycle:\ncontinued:co#80:\\\n\t:li#24:\n";
 assert(write(fd,data,strlen(data))==(ssize_t)strlen(data));assert(!close(fd));setenv("TERMCAP",path,1);
 assert(tgetent(buffer,"alias")==1 && tgetnum("coli")==80 && tgetnum("li")==24 && tgetflag("amco"));
 assert(!strcmp(tgetstr("clcm",&area),"\033[H\033[2J"));
 assert(tgetent(buffer,"child")==1 && tgetnum("li")==25 && !tgetflag("am") && tgetnum("co")==80);
 assert(tgetent(buffer,"cycle")==-1 && errno==EINVAL);
 assert(tgetent(buffer,"continued")==1 && tgetnum("li")==24);
 assert(tgetent(buffer,"absent")==0 && tgetnum("co")==-1);
 setenv("TERMCAP","inline:xx=\\072^G\\7:co#999999999999999999999999:",1);
 assert(tgetent(buffer,"inline")==1);area=strings;
 assert(!strcmp(tgetstr("xx",&area),":\7\7"));assert(tgetnum("co")==-1 && errno==ERANGE);
 char huge[TCBUFSIZE+50];memset(huge,'x',sizeof(huge)-1);huge[sizeof(huge)-1]=0;memcpy(huge,"long:",5);
 setenv("TERMCAP",huge,1);assert(tgetent(buffer,"long")==-1 && errno==E2BIG);
 assert(tgetent(buffer,"")==-1);assert(!unlink(path));puts("PASS: bounded termcap lookup, inheritance, cancellation, escape decoding and failures");
}
