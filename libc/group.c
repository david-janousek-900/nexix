/* Native adapters for V7 stdio/getgrent.c, getgrnam.c and getgrgid.c. */
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#define MAXGRP 100
static FILE *iterator;
static char line[BUFSIZ];
static char *members[MAXGRP];
static struct group entry;
static int record(void)
{
    size_t length=0;int c,overflow=0;
    while((c=getc(iterator))!=EOF){
        if(c=='\n')break;
        if(length<BUFSIZ-1)line[length++]=(char)c;
        else overflow=1;
    }
    if(c==EOF&&ferror(iterator))return 0;
    if(overflow){errno=ERANGE;return -1;}
    if(c==EOF&&!length)return 0;
    line[length]=0;return 1;
}
void setgrent(void)
{ if(iterator)rewind(iterator);else iterator=fopen("/etc/group","r"); }
void endgrent(void)
{ if(iterator){(void)fclose(iterator);iterator=NULL;} }
struct group *getgrent(void)
{
    if(!iterator&&!(iterator=fopen("/etc/group","r")))return NULL;
    int state;
    while((state=record())!=0){
        if(state<0)continue;
        char *fields[4];fields[0]=line;int i;
        for(i=1;i<4;++i){
            char *colon=strchr(fields[i-1],':');if(!colon)break;
            *colon=0;fields[i]=colon+1;
        }
        if(i!=4){errno=EINVAL;continue;}
        size_t count=0;char *p=fields[3];
        while(*p){
            if(count==MAXGRP-1)break;
            members[count++]=p;
            char *comma=strchr(p,',');
            if(!comma){p+=strlen(p);break;}
            *comma=0;p=comma+1;
        }
        if(*p){errno=ERANGE;continue;}
        members[count]=NULL;
        entry=(struct group){fields[0],fields[1],atoi(fields[2]),members};
        return &entry;
    }
    return NULL;
}
struct group *getgrnam(const char *name)
{
    if(!name){errno=EINVAL;return NULL;}
    setgrent();struct group *p;
    while((p=getgrent())&&strcmp(name,p->gr_name)){}
    endgrent();return p;
}
struct group *getgrgid(int gid)
{
    setgrent();struct group *p;
    while((p=getgrent())&&p->gr_gid!=gid){}
    endgrent();return p;
}
