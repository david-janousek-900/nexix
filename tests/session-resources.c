#include <sys/kinfo.h>
#include <stdio.h>
int main(void)
{
    struct {struct nx_kinfo_header header;struct nx_kinfo_memory memory;} snapshot;
    ssize_t process=nx_kinfo(1,NX_KINFO_PROCESS,NULL,0),file=nx_kinfo(1,NX_KINFO_FILE,NULL,0);
    ssize_t inode=nx_kinfo(1,NX_KINFO_INODE,NULL,0),text=nx_kinfo(1,NX_KINFO_TEXT,NULL,0);
    if(process<48 || file<48 || inode<48 || text<48 || nx_kinfo(1,NX_KINFO_MEMORY,&snapshot,sizeof(snapshot))!=sizeof(snapshot))return 1;
    printf("RES %ld %ld %ld %ld %lu\n",(long)process,(long)file,(long)inode,(long)text,(unsigned long)snapshot.memory.free_pages);return 0;
}
