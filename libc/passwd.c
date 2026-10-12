/* Native adapters for V7 stdio/getpwent.c, getpwnam.c, getpwuid.c, getpw.c. */
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
static FILE *iterator;
static char line[BUFSIZ],empty[]="";
static struct passwd entry;
/* Consume an entire logical record, even when too long to retain. Return 1
 * for a bounded record, -1 for an oversized one, 0 for EOF or read failure. */
static int record(FILE *file,char *buffer,int *terminated)
{
    size_t length=0;
    int c,overflow=0;
    *terminated=0;
    while((c=getc(file))!=EOF){
        if(c=='\n'){*terminated=1;break;}
        if(length<BUFSIZ-1)buffer[length++]=(char)c;
        else overflow=1;
    }
    if(c==EOF && ferror(file))return 0;
    if(overflow){errno=ERANGE;return -1;}
    if(c==EOF && !length)return 0;
    buffer[length]=0;
    return 1;
}
void setpwent(void)
{
    if(iterator)rewind(iterator);
    else iterator=fopen("/etc/passwd","r");
}
void endpwent(void)
{
    if(iterator){(void)fclose(iterator);iterator=NULL;}
}
struct passwd *getpwent(void)
{
    if(!iterator && !(iterator=fopen("/etc/passwd","r")))return NULL;
    int newline,state;
    while((state=record(iterator,line,&newline))!=0){
        if(state<0)continue;
        char *fields[7];fields[0]=line;
        int i;
        for(i=1;i<7;++i){
            char *colon=strchr(fields[i-1],':');
            if(!colon)break;
            *colon=0;fields[i]=colon+1;
        }
        if(i!=7){errno=EINVAL;continue;}
        /* V7 uses atoi's prefix conversion, not strict UID token validation. */
        entry=(struct passwd){fields[0],fields[1],atoi(fields[2]),atoi(fields[3]),
                              0,empty,fields[4],fields[5],fields[6]};
        return &entry;
    }
    return NULL;
}
struct passwd *getpwnam(const char *name)
{
    if(!name){errno=EINVAL;return NULL;}
    setpwent();
    struct passwd *p;
    while((p=getpwent()) && strcmp(name,p->pw_name)){}
    endpwent();return p;
}
struct passwd *getpwuid(int uid)
{
    setpwent();
    struct passwd *p;
    while((p=getpwent()) && p->pw_uid!=uid){}
    endpwent();return p;
}
int getpw(int uid,char *buffer)
{
    if(!buffer){errno=EINVAL;return 1;}
    /* Independent stream preserves getpwent's iteration position. Closing
     * after each lookup avoids the original permanently cached descriptor. */
    FILE *file=fopen("/etc/passwd","r");
    if(!file)return 1;
    char raw[BUFSIZ];int newline,state,result=1;
    while((state=record(file,raw,&newline))!=0){
        if(state<0)continue;
        if(!newline)break; /* Original getpw requires a complete newline. */
        char *p=raw;
        int i;
        for(i=0;i<2;++i){p=strchr(p,':');if(!p)break;++p;}
        if(i!=2 || !strchr(p,':')){errno=EINVAL;continue;}
        unsigned value=0;int overflow=0;
        while(*p!=':'){
            if(*p>='0' && *p<='9'){
                unsigned digit=(unsigned)(*p-'0');
                if(value>((unsigned)INT32_MAX-digit)/10)overflow=1;
                else value=value*10+digit;
            }
            ++p;
        }
        if(overflow){errno=ERANGE;continue;}
        if(value==(unsigned)uid){strcpy(buffer,raw);result=0;break;}
    }
    int saved=errno;(void)fclose(file);errno=saved;return result;
}
