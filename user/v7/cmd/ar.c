/* Native archive front end preserving the V7 command/positioning interface. */
#include "archive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
static int verbose;
static const char *base(const char *s) {const char *p=strrchr(s,'/');return p?p+1:s;}
static int usage(void) {fprintf(stderr,"usage: ar [drqtpmxs][vucabislS] [position] archive [file ...]\n");return 2;}
static void message(char op,const char *name) {if(verbose)printf("%c - %s\n",op,name);}
static int append(struct archive *a,struct archive_member m) {if(a->count>=ARCHIVE_MEMBERS)return -1;a->members[a->count++]=m;return 0;}
static int extract(const struct archive_member *m) {
    if(!archive_safe_name(m->name))return -1;
    /* V7 has no symbolic links; the host also rejects symlink targets. */
    struct stat st;if(!stat(m->name,&st)&&!S_ISREG(st.st_mode))return -1;
    int flags=O_WRONLY|O_CREAT|O_TRUNC;
#ifdef O_NOFOLLOW
    flags|=O_NOFOLLOW;
#endif
    int fd=open(m->name,flags,0600);if(fd<0)return -1;
    size_t n=0;int result=0;
    while(n<m->size) {ssize_t k=write(fd,m->data+n,m->size-n);if(k<=0){result=-1;break;}n+=k;}
    if(close(fd))result=-1;
    mode_t mask=umask(0);umask(mask);
    if(!result&&chmod(m->name,(m->mode&0777)&~mask))result=-1;
    return result;
}
int main(int argc,char **argv) {
    char command=0,positioning=0;int update=0,quiet=0,indexed=0,noindex=0;
    if(argc<3)return usage();
    const char *flags=argv[1];if(*flags=='-')flags++;
    for(const char *p=flags;*p;p++) {
        if(strchr("drqtpmx",*p)) {if(command)return usage();command=*p;}
        else switch(*p) {
        case 's':indexed=1;break;case 'S':noindex=1;break;
        case 'v':verbose=1;break;case 'u':update=1;break;case 'c':quiet=1;break;
        case 'a':case 'b':case 'i':if(positioning)return usage();positioning=*p;break;
        case 'l':break;default:return usage();
        }
    }
    if(!command) {if(update)command='r';else if(indexed)command='s';else return usage();}
    if((indexed&&noindex)||(update&&command!='r')||(positioning&&command!='r'&&command!='m'))return usage();
    int at=2;const char *position=NULL;if(positioning) {position=base(argv[at++]);if(at>=argc)return usage();}
    const char *path=argv[at++];int count=argc-at;char **files=argv+at;char **implicit=NULL;
    struct archive old={0},out={0};struct stat st;int exists=!stat(path,&st);
    if(exists) {if(archive_load(path,&old)){fprintf(stderr,"ar: invalid archive %s\n",path);return 1;}}
    else if(command=='r'||command=='q') {old.members=calloc(ARCHIVE_MEMBERS,sizeof(*old.members));if(!old.members)return 1;if(!quiet)fprintf(stderr,"ar: creating %s\n",path);}
    else {fprintf(stderr,"ar: cannot open %s\n",path);return 1;}
    if(command=='r'&&!count&&old.count) {
        implicit=calloc(old.count,sizeof(*implicit));if(!implicit){archive_free(&old);return 1;}
        files=implicit;
        for(size_t i=0;i<old.count;i++)if(!access(old.members[i].name,R_OK))files[count++]=old.members[i].name;
    }
    if(command=='s'&&count){archive_free(&old);return usage();}
    out.members=calloc(ARCHIVE_MEMBERS,sizeof(*out.members));
    unsigned char *used=calloc(count?count:1,1),*selected=calloc(old.count?old.count:1,1);
    struct archive_member *replacements=calloc(count?count:1,sizeof(*replacements));
    int *targets=malloc((count?count:1)*sizeof(*targets));
    int result=1;size_t boundary=old.count;
    if(!out.members||!used||!selected||!replacements||!targets)goto done;
    if(position) {
        int found=0;for(size_t i=0;i<old.count;i++)if(!strcmp(old.members[i].name,position)) {boundary=i+(positioning=='a');found=1;break;}
        if(!found){fprintf(stderr,"ar: position not found: %s\n",position);goto done;}
    }
    if(command=='r'||command=='q') {
        for(int i=0;i<count;i++)if(archive_file(files[i],replacements+i)) {fprintf(stderr,"ar: cannot read %s\n",files[i]);goto done;}
    }
    for(int j=0;j<count;j++) {
        targets[j]=-1;
        for(size_t k=0;k<old.count;k++)if(!strcmp(base(files[j]),old.members[k].name)){targets[j]=(int)k;break;}
    }
    for(size_t i=0;i<old.count;i++) {
        int match=-1;
        for(int j=0;j<count;j++)if(!used[j]&&!strcmp(base(files[j]),old.members[i].name)){match=j;break;}
        if(match>=0) {used[match]=1;selected[i]=1;}
        if(command=='t'||command=='p'||command=='x') {
            if(count && match<0)continue;
            const struct archive_member *m=old.members+i;
            if(command=='t') {if(verbose)printf("%04lo %lu/%lu %lu %lu ",(unsigned long)m->mode,(unsigned long)m->uid,(unsigned long)m->gid,(unsigned long)m->size,(unsigned long)m->date);puts(m->name);}
            if(command=='p') {if(verbose)printf("\n<%s>\n\n",m->name);if(fwrite(m->data,1,m->size,stdout)!=m->size)goto done;}
            if(command=='x') {if(extract(m)){fprintf(stderr,"ar: cannot extract %s\n",m->name);goto done;}message('x',m->name);}
        }
    }
    if(command=='t'||command=='p'||command=='x'||command=='d'||command=='m') {
        for(int j=0;j<count;j++)if(!used[j]) {fprintf(stderr,"ar: member not found: %s\n",files[j]);goto done;}
    }
    if(command=='t'||command=='p'||command=='x') {result=fflush(stdout)||ferror(stdout)?1:0;goto done;}
    memset(used,0,count);
    for(size_t i=0;i<=old.count;i++) {
        if(i==boundary) {
            if(command=='m')for(size_t k=0;k<old.count;k++)if(selected[k]) {struct archive_member m=old.members[k];if(append(&out,m))goto done;memset(old.members+k,0,sizeof(m));message('m',m.name);}
            if(command=='r')for(int j=0;j<count;j++) {
                if(targets[j]<0) {struct archive_member m=replacements[j];if(append(&out,m))goto done;memset(replacements+j,0,sizeof(m));used[j]=1;message('a',m.name);}
            }
        }
        if(i==old.count)break;
        if(command=='d'&&selected[i]) {message('d',old.members[i].name);continue;}
        if(command=='m'&&selected[i])continue;
        if(command=='r')for(int j=0;j<count;j++)if(!used[j]&&targets[j]==(int)i) {
            used[j]=1;
            if(!update||replacements[j].date>old.members[i].date) {
                archive_member_free(old.members+i);old.members[i]=replacements[j];memset(replacements+j,0,sizeof(*replacements));message('r',old.members[i].name);
            }
            break;
        }
        if(append(&out,old.members[i]))goto done;
        memset(old.members+i,0,sizeof(*old.members));
    }
    if(command=='q')for(int j=0;j<count;j++) {struct archive_member m=replacements[j];if(append(&out,m))goto done;memset(replacements+j,0,sizeof(m));message('q',m.name);}
    if(archive_write(path,&out,noindex?0:(indexed||old.indexed))) {fprintf(stderr,"ar: cannot write %s\n",path);goto done;}
    result=0;
done:
    if(replacements)for(int i=0;i<count;i++)archive_member_free(replacements+i);
    free(implicit);free(targets);free(replacements);free(used);free(selected);archive_free(&out);archive_free(&old);return result;
}
