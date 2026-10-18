#ifndef NEXIX_V7_TTY_PATH_H
#define NEXIX_V7_TTY_PATH_H
#include <string.h>
/* Resolve a flat V7 device basename without reading beyond a utmp field. */
static int v7_tty_path(char *output,const char *name,unsigned limit)
{
    unsigned n;
    for(n=0;n<limit && name[n];++n)
        if((unsigned char)name[n]<33 || (unsigned char)name[n]>126 || name[n]=='/')return 0;
    if(!n || (n==1 && name[0]=='.') || (n==2 && name[0]=='.' && name[1]=='.'))return 0;
    memcpy(output,"/dev/",5);memcpy(output+5,name,n);output[5+n]=0;return 1;
}
#endif
