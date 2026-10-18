#include <unistd.h>
#include <pwd.h>
#include <crypt.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
int main(void)
{
    const char *names[]={"alice","root","bob"},*passwords[]={"Reset123","nexix","bob"};struct stat st;
    for(unsigned n=0;n<3;++n){struct passwd *p=getpwnam(names[n]);if(!p || strcmp(crypt(passwords[n],p->pw_passwd),p->pw_passwd))return 1;}
    setpwent();unsigned count=0;while(getpwent())++count;endpwent();if(count!=7)return 2;
    if(stat("/etc/passwd",&st) || st.st_mode!=(S_IFREG|0644) || st.st_uid || st.st_gid || st.st_nlink!=1 || st.st_mtime<1700000000)return 3;
    if(stat("/etc/ptmp",&st)!=-1 || errno!=ENOENT || stat("/etc/opasswd",&st)!=-1 || errno!=ENOENT)return 4;
    if(stat("/bin/passwd",&st) || st.st_mode!=(S_IFREG|04755) || st.st_uid)return 5;
    puts("PASSWD_STATE_OK");return 0;
}
