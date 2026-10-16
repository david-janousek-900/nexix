#include <a.out.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "../tests/nlist-fixture.h"
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
int nlist_test(void)
{
    unsigned char image[FIXTURE_SIZE];fixture(image);
    const char *path="/tmp/nlist-fixture";
    int fd=creat(path,0600);CHECK(fd>=0);
    CHECK(write(fd,image,sizeof(image))==(ssize_t)sizeof(image) && close(fd)==0);
    struct nlist l[]={{"long_symbol_name",0,0},{"common",0,0},{"absent",9,9},{NULL,0,0}};
    CHECK(nlist(path,l)==0 && l[0].n_type==(N_TEXT|N_EXT));
    CHECK(l[0].n_value==UINT64_C(0x1234567800000001));
    CHECK(l[1].n_value==64 && l[1].n_type==N_EXT && !l[2].n_value && !l[2].n_type);
    putle(image+768+7*24,999,4);
    fd=creat(path,0600);CHECK(fd>=0);
    CHECK(write(fd,image,sizeof(image))==(ssize_t)sizeof(image) && close(fd)==0);
    CHECK(nlist(path,l)==-1 && errno==ENOEXEC && !l[0].n_type && !l[0].n_value);
    CHECK(unlink(path)==0);
    struct nlist self[]={{"_start",0,0},{"main",0,0},{NULL,0,0}};
    Elf64_Ehdr h;fd=open("/bin/echo",O_RDONLY);CHECK(fd>=0);
    CHECK(read(fd,&h,sizeof(h))==(ssize_t)sizeof(h) && close(fd)==0);
    CHECK(nlist("/bin/echo",self)==0 && self[0].n_type==(N_TEXT|N_EXT));
    CHECK(self[0].n_value==h.e_entry && self[1].n_type==(N_TEXT|N_EXT) && self[1].n_value);
    for(int i=0;i<20;++i) CHECK(nlist("/missing-nlist",l)==-1 && errno==ENOENT);
    static const char marker[]="NEXIX_NLIST_OK\n";
    CHECK(write(1,marker,sizeof(marker)-1)==sizeof(marker)-1);
    return 0;
}
