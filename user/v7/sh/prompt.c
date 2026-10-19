/* Native PS1 escapes. Resolve passwd before the V7 shell takes ownership of
 * sbrk; prompt rendering itself never allocates or uses stdio. */
#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <string.h>
#include <errno.h>
static char username[128];
void nexix_prompt_init(void)
{
    unsigned uid=geteuid();
    struct passwd *account=getpwuid(uid);
    if(account && account->pw_name && strlen(account->pw_name)<sizeof username)
        strcpy(username,account->pw_name);
    else {
        char digits[10];size_t n=0,i;
        do {digits[n++]=(char)('0'+uid%10);uid/=10;}while(uid);
        for(i=0;i<n;i++)username[i]=digits[n-i-1];
        username[n]=0;
    }
    endpwent();
}
static void emit(int fd,const char *text,size_t size)
{
    while(size) {
        ssize_t n=write(fd,text,size);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)return;
        text+=n;size-=n;
    }
}
static void append(int fd,char buffer[512],size_t *used,const char *text,size_t size)
{
    while(size) {
        size_t part=512-*used;
        if(!part){emit(fd,buffer,*used);*used=0;part=512;}
        if(part>size)part=size;
        memcpy(buffer+*used,text,part);*used+=part;text+=part;size-=part;
    }
}
void nexix_prompt(const char *text,int fd)
{
    struct utsname system;
    char buffer[512];
    size_t used=0;
    const char *value;
    size_t size;
    int known=uname(&system)==0;
    if(!text)return;
    while(*text) {
        if(*text!='\\' || !text[1]){append(fd,buffer,&used,text++,1);continue;}
        text++;
        switch(*text) {
        case 'u':value=username;size=strlen(value);break;
        case 'h':case 'H':
            value=known?system.nodename:"nexix";size=strlen(value);
            if(*text=='h') { const char *dot=strchr(value,'.');if(dot)size=(size_t)(dot-value); }
            break;
        case '$':value=geteuid()==0?"#":"$";size=1;break;
        case '\\':value="\\";size=1;break;
        default:append(fd,buffer,&used,"\\",1);value=text;size=1;break;
        }
        append(fd,buffer,&used,value,size);text++;
    }
    emit(fd,buffer,used);
}
