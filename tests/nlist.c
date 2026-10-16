#include <a.out.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "nlist-fixture.h"
#define CHECK(c) do { if(!(c)) return __LINE__; } while(0)
int errno;
static unsigned char image[FIXTURE_SIZE];
static size_t position,length;
static int opened,closed,failure,interrupted;
int open(const char *p,int flags,...)
{ (void)p;(void)flags; if(failure==1){errno=ENOENT;return -1;} ++opened;position=0;return 7; }
int close(int fd)
{ (void)fd;++closed;if(failure==5){errno=EIO;return -1;}return 0; }
int fstat(int fd,struct stat *st)
{ (void)fd;if(failure==2){errno=EIO;return -1;}st->st_size=length;return 0; }
off_t lseek(int fd,off_t off,int whence)
{ (void)fd;(void)whence;if(failure==3){errno=ESPIPE;return -1;}position=(size_t)off;return off; }
ssize_t read(int fd,void *buffer,size_t n)
{
    (void)fd;
    if(failure==4){errno=EIO;return -1;}
    if(!interrupted++){errno=EINTR;return -1;}
    if(position>=length || failure==6) return 0;
    if(n>length-position)n=length-position;
    if(n>7)n=7; /* Exercise exact reads across every record. */
    memcpy(buffer,image+position,n);position+=n;return n;
}
static void reset(void)
{fixture(image);length=sizeof(image);failure=0;interrupted=0;}
static int rejected(struct nlist *l)
{
    l[0].n_type=99;l[0].n_value=99;
    int rc=nlist("fixture",l);
    return rc==-1 && errno==ENOEXEC && !l[0].n_type && !l[0].n_value;
}
int main(void)
{
    struct nlist l[]={ {"long_symbol_name",99,99},{"data",0,0},{"bss",0,0},
        {"absolute",0,0},{"common",0,0},{"undefined",0,0},{"weak",0,0},
        {"missing",99,99},{"long_symbol_name",0,0},{NULL,0,0} };
    reset();CHECK(nlist("fixture",l)==0);
    CHECK(l[0].n_type==(N_TEXT|N_EXT) && l[0].n_value==UINT64_C(0x1234567800000001));
    CHECK(l[1].n_type==(N_DATA|N_EXT) && l[2].n_type==(N_BSS|N_EXT));
    CHECK(l[3].n_type==(N_ABS|N_EXT));
    CHECK(l[4].n_type==N_EXT && l[4].n_value==64);
    CHECK(l[5].n_type==N_EXT && !l[5].n_value);
    CHECK(l[6].n_type==(N_TEXT|N_EXT));
    CHECK(!l[7].n_type && !l[7].n_value && !l[8].n_value && !l[8].n_type);
    reset();image[768+24+4]=ELF64_ST_INFO(STB_LOCAL,STT_FUNC);
    CHECK(nlist("local",l)==0 && l[0].n_type==N_TEXT);
    reset();image[768+24+4]=ELF64_ST_INFO(STB_LOCAL,STT_FILE);
    CHECK(nlist("file",l)==0 && l[0].n_type==N_FN);
    reset();image[768+24+4]=ELF64_ST_INFO(STB_LOCAL,STT_TLS);
    CHECK(nlist("tls",l)==0 && !l[0].n_type);
    reset();image[768+24+4]=ELF64_ST_INFO(STB_LOCAL,STT_SECTION);
    CHECK(nlist("section",l)==0 && !l[0].n_type);
    /* A full SYMTAB takes precedence over a separate dynamic table. */
    reset();memcpy(image+64+6*64,image+64+5*64,64);
    putle(image+64+6*64+4,11,4);putle(image+64+6*64+24,1024,8);
    memcpy(image+1024,image+768,8*24);putle(image+1024+24+8,17,8);
    CHECK(nlist("both",l)==0 && l[0].n_value==UINT64_C(0x1234567800000001));
    /* Duplicate file symbols overwrite the first request, as V7's scan did. */
    reset();putle(image+768+7*24,1,4);
    CHECK(nlist("duplicate",l)==0 && l[0].n_value==UINT64_C(0x1234567800000007) && !l[8].n_value);
    /* Names cross comparison chunks without truncation or prefix matching. */
    reset();char longname[301];memset(longname,'q',300);longname[300]=0;
    memcpy(image+1024+1,longname,sizeof(longname));image[1024]=0;
    putle(image+64+4*64+24,1024,8);putle(image+64+4*64+32,302,8);
    for(unsigned i=1;i<8;++i)putle(image+768+i*24,1,4);
    struct nlist longlist[]={{longname,0,0},{"qqqq",0,0},{NULL,0,0}};
    CHECK(nlist("long",longlist)==0 && longlist[0].n_value && !longlist[1].n_type);
    reset();putle(image+64+5*64+4,11,4);CHECK(nlist("dynsym",l)==0 && l[0].n_type==(N_TEXT|N_EXT));
    reset();putle(image+16,1,2);CHECK(nlist("rel",l)==0);
    reset();putle(image+16,3,2);CHECK(nlist("dyn",l)==0);
    reset();putle(image+64+5*64+4,0,4);CHECK(nlist("stripped",l)==0 && !l[0].n_type);
    reset();putle(image+40,0,8);putle(image+60,0,2);CHECK(nlist("sectionless",l)==0);
    for(unsigned i=0;i<64;++i){reset();length=i;CHECK(rejected(l));}
    const unsigned offsets[]={0,4,5,6,16,18,20,52,58};
    for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);++i){reset();image[offsets[i]]=255;CHECK(rejected(l));}
    reset();putle(image+40,UINT64_MAX-12,8);CHECK(rejected(l));
    reset();putle(image+60,0,2);CHECK(rejected(l)); /* extended counts unsupported */
    reset();putle(image+62,7,2);CHECK(rejected(l));
    reset();putle(image+64+5*64+24,UINT64_MAX,8);CHECK(rejected(l));
    reset();putle(image+64+5*64+32,193,8);CHECK(rejected(l));
    reset();putle(image+64+5*64+40,7,4);CHECK(rejected(l));
    reset();putle(image+64+5*64+56,23,8);CHECK(rejected(l));
    reset();putle(image+64+4*64+4,1,4);CHECK(rejected(l));
    reset();putle(image+64+4*64+32,UINT64_MAX,8);CHECK(rejected(l));
    reset();image[512]=1;CHECK(rejected(l));
    reset();putle(image+768+7*24,999,4);CHECK(rejected(l)); /* after earlier successful lookups */
    reset();putle(image+768+7*24+6,0xffff,2);CHECK(rejected(l));
    reset();memset(image+512+53,'x',sizeof("weak")+1);CHECK(rejected(l));
    const int errors[]={0,ENOENT,EIO,ESPIPE,EIO,EIO,ENOEXEC};
    for(int f=1;f<=6;++f){reset();failure=f;l[0].n_type=99;l[0].n_value=99;CHECK(nlist("fixture",l)==-1 && errno==errors[f] && !l[0].n_type && !l[0].n_value);}
    reset();CHECK(nlist(NULL,l)==-1 && errno==EINVAL);CHECK(nlist("fixture",NULL)==-1 && errno==EINVAL);
    struct nlist empty[]={{"",0,0}};CHECK(nlist("fixture",empty)==0);
    CHECK(opened==closed);return 0;
}
