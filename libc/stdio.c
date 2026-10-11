/* Integer-only, unbuffered streams for native V7 commands. */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
static FILE streams[20] = {{0,1,0,0}, {1,1,0,0}, {2,1,0,0}};
FILE *stdin = &streams[0], *stdout = &streams[1], *stderr = &streams[2];
FILE *fopen(const char *path, const char *mode)
{
    int flags;
    if (!strcmp(mode,"r")) flags=O_RDONLY;
    else if (!strcmp(mode,"w")) flags=O_WRONLY|O_CREAT|O_TRUNC;
    else if (!strcmp(mode,"a")) flags=O_WRONLY|O_CREAT|O_APPEND;
    else if (!strcmp(mode,"r+")) flags=O_RDWR;
    else if (!strcmp(mode,"w+")) flags=O_RDWR|O_CREAT|O_TRUNC;
    else if (!strcmp(mode,"a+")) flags=O_RDWR|O_CREAT|O_APPEND;
    else { errno=EINVAL; return NULL; }
    for (unsigned i=0;i<20;++i) if (!streams[i].used) {
        int fd=open(path,flags,0666); if(fd<0)return NULL;
        streams[i]=(FILE){fd,1,0,0}; return &streams[i];
    }
    errno=EMFILE;return NULL;
}
int fclose(FILE *f) { int result=close(f->fd); f->used=0;return result; }
int fflush(FILE *f) { (void)f; return 0; }
void setbuf(FILE *f,char *b) { (void)f;(void)b; }
int fileno(FILE *f) { return f->fd; }
size_t fread(void *buf,size_t size,size_t count,FILE *f)
{
    if (!size || !count) return 0;
    if(count>SIZE_MAX/size){errno=EINVAL;f->error=1;return 0;}
    size_t total=size*count,done=0;
    while(done<total){ssize_t n=read(f->fd,(char *)buf+done,total-done);
        if(n<0){if(errno==EINTR)continue;f->error=1;break;}
        if(!n){f->eof=1;break;}done+=n;}
    return done/size;
}
size_t fwrite(const void *buf,size_t size,size_t count,FILE *f)
{
    if (!size || !count) return 0;
    if(count>SIZE_MAX/size){errno=EINVAL;f->error=1;return 0;}
    size_t total=size*count,done=0;
    while(done<total){ssize_t n=write(f->fd,(const char *)buf+done,total-done);
        if(n<0&&errno==EINTR)continue;
        if(n<=0){f->error=1;break;}done+=n;}
    return done/size;
}
int fgetc(FILE *f){unsigned char c;return fread(&c,1,1,f)==1?c:EOF;}
int getc(FILE *f){return fgetc(f);}
int getchar(void){return fgetc(stdin);}
int fputc(int c,FILE *f){unsigned char b=c;return fwrite(&b,1,1,f)==1?b:EOF;}
int putc(int c,FILE *f){return fputc(c,f);}
int putchar(int c){return fputc(c,stdout);}
int fputs(const char *s,FILE *f){size_t n=strlen(s);return fwrite(s,1,n,f)==n?0:EOF;}
int puts(const char *s){return fputs(s,stdout)<0?EOF:fputc('\n',stdout);}
char *fgets(char *s,int size,FILE *f){int i=0,c;if(size<=0)return NULL;while(i<size-1&&(c=fgetc(f))!=EOF){s[i++]=c;if(c=='\n')break;}s[i]=0;return i?s:NULL;}
int fseek(FILE *f,long offset,int whence){if(lseek(f->fd,offset,whence)<0)return -1;f->eof=0;return 0;}
long ftell(FILE *f){return lseek(f->fd,0,SEEK_CUR);}
void rewind(FILE *f){(void)fseek(f,0,SEEK_SET);f->error=0;}
int feof(FILE *f){return f->eof;}
int ferror(FILE *f){return f->error;}
void clearerr(FILE *f){f->error=f->eof=0;}
struct sink { FILE *file;char *buffer;size_t capacity,count;int failed; };
static void emit(struct sink *s,char c){if(s->file){if(fputc(c,s->file)==EOF)s->failed=1;}else if(s->count+1<s->capacity)s->buffer[s->count]=c;++s->count;}
static int format(struct sink *s,const char *fmt,va_list ap)
{
    while(*fmt){if(*fmt!='%'){emit(s,*fmt++);continue;}++fmt;
        int left=0,zero=0,width=0,precision=-1,wide=0;
        if(*fmt=='-'){left=1;++fmt;}if(*fmt=='0'){zero=1;++fmt;}
        while(*fmt>='0'&&*fmt<='9'){if(width<4096)width=width*10+*fmt-'0';++fmt;}
        if(*fmt=='.'){precision=0;++fmt;while(*fmt>='0'&&*fmt<='9'){if(precision<4096)precision=precision*10+*fmt-'0';++fmt;}}
        if(*fmt=='l'){wide=1;++fmt;}
        char kind=*fmt;if(!kind)break;++fmt;
        char digits[32],*p=digits+sizeof(digits);const char *text;size_t n;char sign=0;
        if(kind=='s'){text=va_arg(ap,const char *);if(!text)text="(null)";n=0;while(text[n]&&(precision<0||n<(size_t)precision))++n;}
        else if(kind=='c'){digits[0]=va_arg(ap,int);text=digits;n=1;}
        else if(kind=='%'){digits[0]='%';text=digits;n=1;}
        else if(kind=='d'||kind=='i'||kind=='D'||kind=='u'||kind=='o'||kind=='O'||kind=='x'||kind=='X'){
            int signed_value=kind=='d'||kind=='i'||kind=='D';unsigned base=kind=='o'||kind=='O'?8:kind=='x'||kind=='X'?16:10;
            uint64_t value;
            if(signed_value){int64_t v=wide||kind=='D'?va_arg(ap,long):va_arg(ap,int);if(v<0){sign='-';value=0-(uint64_t)v;}else value=v;}
            else value=wide||kind=='O'?va_arg(ap,unsigned long):va_arg(ap,unsigned int);
            do{unsigned d=value%base;*--p=d<10?'0'+d:(kind=='X'?'A':'a')+d-10;value/=base;}while(value);
            text=p;n=digits+sizeof(digits)-p;
        }else{errno=EINVAL;return -1;}
        size_t pad=width>(int)(n+(sign!=0))?width-n-(sign!=0):0;
        if(!left&&!zero)while(pad){emit(s,' ');--pad;}
        if(sign)emit(s,sign);
        if(!left&&zero)while(pad){emit(s,'0');--pad;}
        for(size_t i=0;i<n;++i)emit(s,text[i]);
        if(left)while(pad){emit(s,' ');--pad;}
    }
    if(!s->file&&s->capacity)s->buffer[s->count<s->capacity?s->count:s->capacity-1]=0;
    return s->failed||s->count>INT32_MAX?-1:(int)s->count;
}
int vfprintf(FILE *f,const char *fmt,va_list ap){struct sink s={.file=f};return format(&s,fmt,ap);}
int fprintf(FILE *f,const char *fmt,...){va_list ap;va_start(ap,fmt);int n=vfprintf(f,fmt,ap);va_end(ap);return n;}
int printf(const char *fmt,...){va_list ap;va_start(ap,fmt);int n=vfprintf(stdout,fmt,ap);va_end(ap);return n;}
int vsnprintf(char *b,size_t cap,const char *fmt,va_list ap){struct sink s={.buffer=b,.capacity=cap};return format(&s,fmt,ap);}
int snprintf(char *b,size_t cap,const char *fmt,...){va_list ap;va_start(ap,fmt);int n=vsnprintf(b,cap,fmt,ap);va_end(ap);return n;}
int sprintf(char *b,const char *fmt,...){va_list ap;va_start(ap,fmt);int n=vsnprintf(b,SIZE_MAX,fmt,ap);va_end(ap);return n;}
void perror(const char *s){int error=errno;if(s&&*s)fprintf(stderr,"%s: ",s);fprintf(stderr,"error %d\n",error);errno=error;}
