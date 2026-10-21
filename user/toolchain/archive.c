/* Explicit on-disk layouts; never overlay native structs on V7 records. */
#include "archive.h"
#include "object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
static const void *findzero(const void *ptr,size_t n) {const unsigned char *p=ptr;for(size_t i=0;i<n;i++)if(!p[i])return p+i;return NULL;}
static char *duplicate(const char *p,size_t n) {
    char *s=malloc(n+1);if(s){memcpy(s,p,n);s[n]=0;}return s;
}
static int number(const unsigned char *p,size_t n,unsigned base,uint64_t *out) {
    size_t i=0;uint64_t v=0;
    while(i<n&&p[i]==' ')i++;
    int seen=0;
    for(;i<n&&p[i]!=' ';i++) {
        unsigned d=p[i]-'0';if(d>=base||v>(UINT64_MAX-d)/base)return -1;
        v=v*base+d;seen=1;
    }
    while(i<n&&p[i]==' ')i++;
    if(i!=n||!seen)return -1;
    *out=v;return 0;
}
static unsigned word(const unsigned char *p) {return p[0]|(unsigned)p[1]<<8;}
static uint32_t pdp(const unsigned char *p) {return (uint32_t)word(p)<<16|word(p+2);}
int archive_safe_name(const char *s) {
    return *s && strcmp(s,".") && strcmp(s,"..") && !strchr(s,'/') && !strchr(s,'\n') && !strchr(s,'\r');
}
void archive_member_free(struct archive_member *m) {free(m->name);free(m->data);memset(m,0,sizeof(*m));}
void archive_free(struct archive *a) {
    for(size_t i=0;i<a->count;i++)archive_member_free(a->members+i);
    free(a->members);memset(a,0,sizeof(*a));
}
int archive_file(const char *path,struct archive_member *m) {
    struct stat st;memset(m,0,sizeof(*m));
    FILE *f=fopen(path,"r");if(!f)return -1;
    int result=-1;
    if(fstat(fileno(f),&st)||!S_ISREG(st.st_mode)||st.st_size<0||(uint64_t)st.st_size>ARCHIVE_LIMIT)goto out;
    const char *name=strrchr(path,'/');name=name?name+1:path;
    if(!archive_safe_name(name)||strlen(name)>4096)goto out;
    m->size=(size_t)st.st_size;m->data=malloc(m->size?m->size:1);m->name=duplicate(name,strlen(name));
    if(!m->data||!m->name||fread(m->data,1,m->size,f)!=m->size||ferror(f))goto out;
    m->date=st.st_mtime<0?0:(uint64_t)st.st_mtime;m->uid=st.st_uid;m->gid=st.st_gid;m->mode=st.st_mode&07777;
    result=0;
out:
    if(fclose(f))result=-1;
    if(result)archive_member_free(m);
    return result;
}
int archive_load(const char *path,struct archive *a) {
    struct archive_member input;memset(a,0,sizeof(*a));
    if(archive_file(path,&input))return -1;
    const unsigned char *data=input.data,*strings=NULL;size_t size=input.size,pos=0,stringsize=0;
    int result=-1;
    if(size>=8&&!memcmp(data,"!<arch>\n",8)) {a->format=1;pos=8;}
    else if(size>=2&&word(data)==0177545) {a->format=2;pos=2;}
    else if(size>=2&&word(data)==0177555) {a->format=3;pos=2;}
    else goto out;
    a->members=calloc(ARCHIVE_MEMBERS,sizeof(*a->members));if(!a->members)goto out;
    while(pos<size) {
        struct archive_member m={0};uint64_t bytes=0,extended=0;size_t header=a->format==1?60:a->format==2?26:16;
        char name[4097];size_t n=0;
        if(header>size-pos)goto out;
        const unsigned char *h=data+pos;
        if(a->format==1) {
            if(memcmp(h+58,"`\n",2)||number(h+48,10,10,&bytes))goto out;
            memcpy(name,h,16);n=16;while(n&&name[n-1]==' ')n--;name[n]=0;if(findzero(name,n))goto out;
            /* GNU index/name-table headers conventionally leave metadata blank. */
            if(strcmp(name,"/")&&strcmp(name,"//")&&strcmp(name,"/SYM64/")) {
                if(number(h+16,12,10,&m.date)||number(h+28,6,10,&m.uid)||
                   number(h+34,6,10,&m.gid)||number(h+40,8,8,&m.mode))goto out;
            }
        } else {
            size_t width=a->format==2?14:8;
            while(n<width&&h[n])n++;
            memcpy(name,h,n);name[n]=0;
            m.date=pdp(h+width);m.uid=h[width+4];
            if(a->format==2) {m.gid=h[19];m.mode=word(h+20);bytes=pdp(h+22);}
            else {m.gid=1;m.mode=h[13];bytes=word(h+14);}
        }
        size_t payload=pos+header;
        if(bytes>size-payload)goto out;
        size_t next=payload+(size_t)bytes;
        if(bytes&1) {if(next==size)goto out;next++;}
        if(a->format==1&&!strcmp(name,"//")) {strings=data+payload;stringsize=bytes;pos=next;continue;}
        if(a->format==1&&(!strcmp(name,"/")||!strcmp(name,"/SYM64/"))) {a->indexed=1;pos=next;continue;}
        if(a->format==1&&!strncmp(name,"#1/",3)) {
            if(number((const unsigned char*)name+3,strlen(name)-3,10,&extended)||extended>bytes||extended>4096)goto out;
            n=extended;while(n&&data[payload+n-1]==0)n--;
            if(findzero(data+payload,n))goto out;
            memcpy(name,data+payload,n);name[n]=0;payload+=extended;bytes-=extended;
        } else if(a->format==1&&name[0]=='/') {
            uint64_t off;if(number((const unsigned char*)name+1,strlen(name)-1,10,&off)||off>=stringsize)goto out;
            n=0;while(off+n<stringsize&&strings[off+n]!='\n') {if(n>=4096)goto out;n++;}
            if(off+n==stringsize)goto out;
            if(n&&strings[off+n-1]=='/')n--;
            if(findzero(strings+off,n))goto out;
            memcpy(name,strings+off,n);name[n]=0;
        } else if(a->format==1&&n&&name[n-1]=='/')name[--n]=0;
        if(!strcmp(name,"__.SYMDEF")||!strcmp(name,"__.SYMDEF SORTED")) {a->indexed=1;pos=next;continue;}
        if(!archive_safe_name(name)||a->count>=ARCHIVE_MEMBERS)goto out;
        m.name=duplicate(name,strlen(name));m.data=malloc(bytes?bytes:1);m.size=bytes;
        if(!m.name||!m.data) {archive_member_free(&m);goto out;}
        memcpy(m.data,data+payload,m.size);a->members[a->count++]=m;pos=next;
    }
    result=0;
out:
    archive_member_free(&input);if(result)archive_free(a);return result;
}
struct index_entry {char *name;size_t member;};
static int put(int fd,const void *p,size_t n) {
    const unsigned char *s=p;
    while(n) {ssize_t k=write(fd,s,n);if(k<0&&errno==EINTR)continue;if(k<=0)return -1;s+=k;n-=k;}return 0;
}
static int field(unsigned char *p,size_t width,uint64_t v,int octal) {
    char text[32];int n=snprintf(text,sizeof(text),octal?"%lo":"%lu",(unsigned long)v);
    if(n<0||(size_t)n>width)return -1;
    memcpy(p,text,n);return 0;
}
static int header(int fd,const char *name,size_t size,const struct archive_member *m) {
    unsigned char h[60];memset(h,' ',sizeof(h));size_t n=strlen(name);if(n>16)return -1;memcpy(h,name,n);
    if(field(h+16,12,m?m->date:0,0)||field(h+28,6,m?m->uid:0,0)||field(h+34,6,m?m->gid:0,0)||
       field(h+40,8,m?m->mode:0,1)||field(h+48,10,size,0))return -1;
    h[58]='`';h[59]='\n';return put(fd,h,sizeof(h));
}
static int bigword(int fd,uint32_t n) {unsigned char b[]={n>>24,n>>16,n>>8,n};return put(fd,b,4);}
static int padding(int fd,size_t size) {return size&1?put(fd,"\n",1):0;}
/* Same-directory hard links implement replacement on V7, which has no rename.
 * The backup remains available until the complete new archive has been linked. */
int archive_write(const char *path,const struct archive *a,int indexed) {
    void (*handlers[4])(int);int signals[]={SIGHUP,SIGINT,SIGQUIT,SIGTERM};
    for(unsigned i=0;i<4;i++)handlers[i]=signal(signals[i],SIG_IGN);
    struct index_entry *index=NULL;size_t capacity=0,count=0,namesize=0,stringbytes=0,total=8;
    size_t *offsets=calloc(a->count?a->count:1,sizeof(*offsets));
    size_t *longoff=calloc(a->count?a->count:1,sizeof(*longoff));
    const char *stage="archive validation";
    struct stat old;int exists=!stat(path,&old),fd=-1,result=-1;char temp[4096],backup[4096];
    if(!offsets||!longoff||strlen(path)>4000)goto out;
    const char *slash=strrchr(path,'/');size_t dir=slash?(size_t)(slash-path+1):0;
    if(exists&&(!S_ISREG(old.st_mode)||access(path,W_OK)))goto out;
    for(size_t i=0;i<a->count;i++) {
        const struct archive_member *m=a->members+i;
        if(!archive_safe_name(m->name)||m->size>ARCHIVE_LIMIT)goto out;
        longoff[i]=stringbytes;
        if(strlen(m->name)>15||strchr(m->name,' '))stringbytes+=strlen(m->name)+2;
        if(indexed && m->size>=4&&!memcmp(m->data,ELFMAG,4)) {
            struct object o;struct object_symbol *symbols=NULL;size_t used;
            if(object_open(&o,m->data,m->size)||object_symbols(&o,&symbols,&used))goto out;
            for(size_t j=0;j<used;j++) {
                struct object_symbol *s=symbols+j;
                if((s->bind!=STB_GLOBAL&&s->bind!=STB_WEAK)||strchr("uUwvf",s->type))continue;
                if(count>=100000) {free(symbols);goto out;}
                if(count==capacity) {
                    capacity=capacity?capacity*2:64;
                    struct index_entry *next=realloc(index,capacity*sizeof(*index));
                    if(!next) {free(symbols);goto out;}index=next;
                }
                index[count].name=duplicate(s->name,strlen(s->name));index[count].member=i;
                if(!index[count].name) {free(symbols);goto out;}
                namesize+=strlen(s->name)+1;count++;
            }
            free(symbols);
        }
    }
    size_t indexsize=4+count*4+namesize;
    if(indexed)total+=60+indexsize+(indexsize&1);
    if(stringbytes)total+=60+stringbytes+(stringbytes&1);
    for(size_t i=0;i<a->count;i++) {offsets[i]=total;total+=60+a->members[i].size+(a->members[i].size&1);if(total>ARCHIVE_LIMIT)goto out;}
    stage="archive temporary create";
    for(unsigned i=0;i<100;i++) {
        if(snprintf(temp,sizeof(temp),"%.*s.nxa%05u%02u",(int)dir,path,(unsigned)getpid()%100000,i)>= (int)sizeof(temp))goto out;
        fd=open(temp,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd>=0)break;if(errno!=EEXIST)goto out;
    }
    if(fd<0)goto out;
    stage="archive write";
    if(put(fd,"!<arch>\n",8))goto cleanup;
    if(indexed) {
        if(header(fd,"/",indexsize,NULL)||bigword(fd,count))goto cleanup;
        for(size_t i=0;i<count;i++)if(bigword(fd,offsets[index[i].member]))goto cleanup;
        for(size_t i=0;i<count;i++)if(put(fd,index[i].name,strlen(index[i].name)+1))goto cleanup;
        if(padding(fd,indexsize))goto cleanup;
    }
    if(stringbytes) {
        if(header(fd,"//",stringbytes,NULL))goto cleanup;
        for(size_t i=0;i<a->count;i++) {const char *n=a->members[i].name;if(strlen(n)>15||strchr(n,' '))if(put(fd,n,strlen(n))||put(fd,"/\n",2))goto cleanup;}
        if(padding(fd,stringbytes))goto cleanup;
    }
    for(size_t i=0;i<a->count;i++) {
        const struct archive_member *m=a->members+i;char name[32];
        if(strlen(m->name)>15||strchr(m->name,' '))snprintf(name,sizeof(name),"/%lu",(unsigned long)longoff[i]);
        else snprintf(name,sizeof(name),"%s/",m->name);
        if(header(fd,name,m->size,m)||put(fd,m->data,m->size)||padding(fd,m->size))goto cleanup;
    }
    stage="archive close";
    if(close(fd)) {fd=-1;goto cleanup;}fd=-1;
    stage="archive chmod";
    mode_t mask=umask(0);umask(mask);
    if(chmod(temp,exists?(old.st_mode&07777):(0666&~mask)))goto cleanup;
    stage="archive replace";
    if(exists) {
        int linked=0;
        for(unsigned i=0;i<100;i++) {
            snprintf(backup,sizeof(backup),"%.*s.nxb%05u%02u",(int)dir,path,(unsigned)getpid()%100000,i);
            if(!link(path,backup)) {linked=1;break;}if(errno!=EEXIST)goto cleanup;
        }
        if(!linked)goto cleanup;
        if(unlink(path)) {unlink(backup);goto cleanup;}
        if(link(temp,path)) {
            if(link(backup,path))fprintf(stderr,"archive: recovery copy is %s\n",backup);
            else unlink(backup);
            goto cleanup;
        }
        unlink(backup);
    } else if(link(temp,path))goto cleanup;
    result=0;
cleanup:
    if(result)perror(stage);
    if(fd>=0)close(fd);
    unlink(temp);
out:
    for(size_t i=0;i<count;i++)free(index[i].name);
    free(index);free(offsets);free(longoff);
    for(unsigned i=0;i<4;i++)if(handlers[i]!=SIG_ERR)signal(signals[i],handlers[i]);
    return result;
}
