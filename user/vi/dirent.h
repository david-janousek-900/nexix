#ifndef NX_VI_DIRENT_H
#define NX_VI_DIRENT_H
#include <stdint.h>
struct dirent { uint16_t d_ino; char d_name[15]; };
typedef struct { int fd; struct dirent entry; } DIR;
DIR *nx_vi_opendir(const char *);
struct dirent *nx_vi_readdir(DIR *);
int nx_vi_closedir(DIR *);
#define opendir nx_vi_opendir
#define readdir nx_vi_readdir
#define closedir nx_vi_closedir
#endif
