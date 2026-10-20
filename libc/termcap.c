/* Native bounded implementation of the traditional BSD termcap contract. */
#include "include/termcap.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
static char capabilities[TCBUFSIZE];
static int name_matches(const char *entry,const char *name)
{
    const char *end=strchr(entry,':');if(!end)return 0;
    while(entry<end){const char *p=entry;while(p<end && *p!='|')++p;
        if((size_t)(p-entry)==strlen(name) && !memcmp(entry,name,p-entry))return 1;
        entry=p+1;
    }return 0;
}
static int lookup(char *out,const char *name)
{
    const char *setting=getenv("TERMCAP");
    if(setting && *setting && *setting!='/' && name_matches(setting,name)){
        size_t n=strlen(setting);if(n>=TCBUFSIZE){errno=E2BIG;return -1;}
        memcpy(out,setting,n+1);return 1;
    }
    int fd=open(setting && *setting=='/'?setting:"/etc/termcap",O_RDONLY);
    if(fd<0)return -1;
    size_t used=0;int overflow=0,leading=1,result=0;char bytes[128];
    for(;;){
        ssize_t count=read(fd,bytes,sizeof(bytes));
        if(count<0){if(errno==EINTR)continue;result=-1;break;}
        for(ssize_t i=0;i<count;++i){char c=bytes[i];
            if(c=='\n'){
                if(used && out[used-1]=='\\' && !overflow){--used;leading=1;continue;}
                if(overflow){errno=E2BIG;result=-1;goto done;}
                out[used]=0;
                if(out[0]!='#' && name_matches(out,name)){result=1;goto done;}
                used=0;leading=1;continue;
            }
            if(leading && (c==' ' || c=='\t'))continue;
            leading=0;
            if(used<TCBUFSIZE-1)out[used++]=c;else overflow=1;
        }
        if(!count){
            if(overflow){errno=E2BIG;result=-1;}
            else {out[used]=0;result=used && out[0]!='#' && name_matches(out,name);}
            break;
        }
    }
done: {int saved=errno;close(fd);errno=saved;}return result;
}
static const char *field_end(const char *p)
{
    while(*p && *p!=':'){if(*p=='\\' && p[1])++p;++p;}return p;
}
int tgetent(char *bp,const char *name)
{
    char names[16][64],entry[TCBUFSIZE],result[TCBUFSIZE];
    unsigned pending=1,read_at=0;size_t used=0;
    capabilities[0]=0;
    if(!bp || !name || !*name || strlen(name)>=sizeof(names[0])){errno=EINVAL;return -1;}
    strcpy(names[0],name);
    while(read_at<pending){
        int found=lookup(entry,names[read_at++]);
        if(found!=1){if(read_at==1)return found;errno=EINVAL;return -1;}
        const char *p=strchr(entry,':');if(!p){errno=EINVAL;return -1;}
        if(!used){size_t n=(size_t)(p-entry)+1;memcpy(result,entry,n);used=n;}
        ++p;
        while(*p){const char *end=field_end(p);size_t n=(size_t)(end-p);
            if(n>=3 && !memcmp(p,"tc=",3)){
                if(pending==16 || n-3>=sizeof(names[0]) || n==3){errno=EINVAL;return -1;}
                memcpy(names[pending],p+3,n-3);names[pending++][n-3]=0;
            }else if(n){
                if(n+1>=sizeof(result)-used){errno=E2BIG;return -1;}
                memcpy(result+used,p,n);used+=n;result[used++]=':';
            }
            p=*end?end+1:end;
        }
    }
    result[used]=0;memcpy(capabilities,result,used+1);memcpy(bp,result,used+1);return 1;
}
static const char *capability(char *id)
{
    if(!id || strlen(id)!=2){errno=EINVAL;return NULL;}
    const char *p=strchr(capabilities,':');if(!p)return NULL;++p;
    while(*p){const char *end=field_end(p);
        if(end-p>=2 && p[0]==id[0] && p[1]==id[1])return p+2;
        p=*end?end+1:end;
    }return NULL;
}
int tgetnum(char *id)
{
    const char *p=capability(id);if(!p || *p!='#')return -1;
    ++p;int base=*p=='0'?8:10,value=0;
    while(*p>='0' && *p<='9'){
        unsigned digit=(unsigned)(*p++-'0');
        if(digit>=(unsigned)base || value>(INT_MAX-(int)digit)/base){errno=ERANGE;return -1;}
        value=value*base+(int)digit;
    }return value;
}
int tgetflag(char *id){const char *p=capability(id);return p && (!*p || *p==':');}
char *tgetstr(char *id,char **area)
{
    const char *p=capability(id);if(!p || *p!='=')return NULL;
    if(!area || !*area){errno=EINVAL;return NULL;}
    char *start=*area,*out=start;++p;const char *end=field_end(p);
    while(p<end){unsigned char c=*p++;
        if(c=='^' && p<end)c=*p++&037;
        else if(c=='\\' && p<end){c=*p++;
            if(c>='0' && c<='7'){
                unsigned value=c-'0',digits=1;
                while(digits<3 && p<end && *p>='0' && *p<='7'){value=value*8+*p++-'0';++digits;}
                c=(unsigned char)value;
            }else if(c=='E' || c=='e')c=27;
            else if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';
            else if(c=='b')c='\b';else if(c=='f')c='\f';
        }
        *out++=(char)c;
    }
    *out++=0;*area=out;return start;
}
