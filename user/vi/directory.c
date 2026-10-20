/* Private release adapter over the native V7 16-byte directory records. */
#include "dirent.h"
#include <sys/dir.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
DIR *nx_vi_opendir(const char *path)
{
 int fd=open(path,O_RDONLY);struct stat st;DIR *d;
 if(fd<0)return 0;
 if(fstat(fd,&st)<0){int e=errno;close(fd);errno=e;return 0;}
 if(!S_ISDIR(st.st_mode)){close(fd);errno=ENOTDIR;return 0;}
 d=malloc(sizeof(*d));if(!d){close(fd);errno=ENOMEM;return 0;}d->fd=fd;return d;
}
struct dirent *nx_vi_readdir(DIR *d)
{
 struct direct raw;
 for(;;){int n=read(d->fd,&raw,sizeof(raw));if(n!=sizeof(raw)){if(n>0)errno=EIO;return 0;}if(!raw.d_ino)continue;
 d->entry.d_ino=raw.d_ino;memcpy(d->entry.d_name,raw.d_name,14);d->entry.d_name[14]=0;return &d->entry;}
}
int nx_vi_closedir(DIR *d){int result=close(d->fd);free(d);return result;}
