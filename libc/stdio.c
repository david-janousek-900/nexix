/* Buffered native streams and V7 integer/floating-point formatting. */
#include <stdio.h>
#include <nexix/stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
static FILE streams[20] = {{.fd=0,.used=1,.readable=1}, {.fd=1,.used=1,.writable=1}, {.fd=2,.used=1,.writable=1,.buffering=-1}};
FILE *stdin = &streams[0], *stdout = &streams[1], *stderr = &streams[2];
static int mode_flags(const char *mode)
{
    int flags;
    if (!mode) { errno=EINVAL; return -1; }
    if (!strcmp(mode,"r")) flags=O_RDONLY;
    else if (!strcmp(mode,"w")) flags=O_WRONLY|O_CREAT|O_TRUNC;
    else if (!strcmp(mode,"a")) flags=O_WRONLY|O_CREAT|O_APPEND;
    else if (!strcmp(mode,"r+")) flags=O_RDWR;
    else if (!strcmp(mode,"w+")) flags=O_RDWR|O_CREAT|O_TRUNC;
    else if (!strcmp(mode,"a+")) flags=O_RDWR|O_CREAT|O_APPEND;
    else { errno=EINVAL; return -1; }
    return flags;
}
static void stream_init(FILE *f, int fd, int flags)
{ *f=(FILE){.fd=fd,.used=1,.readable=(flags&O_ACCMODE)!=O_WRONLY,.writable=(flags&O_ACCMODE)!=O_RDONLY,.append=!!(flags&O_APPEND)}; }
FILE *fopen(const char *path, const char *mode)
{
    int flags=mode_flags(mode); if(flags<0)return NULL;
    for (unsigned i=0;i<20;++i) if (!streams[i].used) {
        int fd=open(path,flags,0666); if(fd<0)return NULL;
        stream_init(&streams[i],fd,flags); return &streams[i];
    }
    errno=EMFILE;return NULL;
}
FILE *fdopen(int fd,const char *mode)
{
    int flags=mode_flags(mode);if(flags<0)return NULL;
    int actual=fcntl(fd,F_GETFL);if(actual<0)return NULL;
    int wanted=flags&O_ACCMODE,access=actual&O_ACCMODE;
    if((wanted==O_RDWR&&access!=O_RDWR)||(wanted==O_RDONLY&&access==O_WRONLY)||
       (wanted==O_WRONLY&&access==O_RDONLY)){errno=EINVAL;return NULL;}
    for(unsigned i=0;i<20;++i)if(!streams[i].used){
        if((flags&O_APPEND)&&fcntl(fd,F_SETFL,actual|O_APPEND)<0)return NULL;
        stream_init(&streams[i],fd,flags);return &streams[i];
    }
    errno=EMFILE;return NULL;
}
FILE *freopen(const char *path,const char *mode,FILE *f)
{
    if(!f||!f->used){errno=EBADF;return NULL;}
    (void)fclose(f);
    int flags=mode_flags(mode);if(flags<0)return NULL;
    int fd=open(path,flags,0666);if(fd<0)return NULL;
    stream_init(f,fd,flags);return f;
}
/* buffering: 0 lazy default, 1 buffered, -1 unbuffered;
 * direction: 0 neutral, 1 reading, 2 writing. */
static int valid(FILE *f)
{
    if(f && f->used)return 1;
    errno=EBADF;if(f)f->error=1;return 0;
}
static void buffer_init(FILE *f,int writing)
{
    if(f->buffering)return;
    int saved=errno;
    if(writing && isatty(f->fd))f->buffering=-1;
    else {
        f->buffer=malloc(BUFSIZ);
        if(f->buffer){f->owned=1;f->buffering=1;}
        else f->buffering=-1; /* V7 falls back to unbuffered I/O. */
    }
    errno=saved;
}
static int flush_output(FILE *f)
{
    if(f->direction!=2)return 0;
    size_t done=0;
    while(done<f->length){
        ssize_t n=write(f->fd,f->buffer+done,f->length-done);
        if(n<0 && errno==EINTR)continue;
        if(n<=0){
            if(!n)errno=EIO;
            memmove(f->buffer,f->buffer+done,f->length-done);
            f->length-=done;f->error=1;return EOF;
        }
        done+=(size_t)n;
    }
    f->position=f->length=0;return 0;
}
int fflush(FILE *f)
{
    if(f){if(!valid(f))return EOF;return flush_output(f);}
    int result=0;
    for(unsigned i=0;i<20;++i)
        if(streams[i].used && flush_output(&streams[i])==EOF)result=EOF;
    return result;
}
int fclose(FILE *f)
{
    if(!valid(f))return EOF;
    int result=flush_output(f),saved=errno;
    if(close(f->fd)<0){result=EOF;saved=errno;}
    if(f->owned)free(f->buffer);
    *f=(FILE){0};
    if(result==EOF)errno=saved;
    return result;
}
void setbuf(FILE *f,char *b)
{
    if(!valid(f))return;
    /* V7 requires this call before I/O. Do not discard pending output if
     * a caller changes an already active stream and flushing fails. */
    if(flush_output(f)==EOF)return;
    if(f->direction==1){
        size_t unread=f->length-f->position+f->pushed;
        if(unread && lseek(f->fd,-(long)unread,SEEK_CUR)<0){f->error=1;return;}
    }
    if(f->owned)free(f->buffer);
    f->buffer=(unsigned char *)b;f->owned=0;f->buffering=b?1:-1;
    f->position=f->length=0;f->direction=f->pushed=0;
}
int fileno(FILE *f) { return f->fd; }
static int direction(FILE *f,int writing)
{
    if(!valid(f))return EOF;
    if(writing?!f->writable:!f->readable){errno=EBADF;f->error=1;return EOF;}
    int next=writing?2:1;
    if(f->direction!=next){
        if(f->direction==2 && flush_output(f)==EOF)return EOF;
        if(f->direction==1){
            size_t unread=f->length-f->position+f->pushed;
            if(unread && lseek(f->fd,-(long)unread,SEEK_CUR)<0){f->error=1;return EOF;}
        }
        f->position=f->length=0;f->pushed=0;f->direction=next;
        if(writing)f->eof=0;
    }
    buffer_init(f,writing);return 0;
}
size_t fread(void *buf,size_t size,size_t count,FILE *f)
{
    if(!size || !count)return 0;
    if(direction(f,0)==EOF)return 0;
    if(count>SIZE_MAX/size){errno=EINVAL;f->error=1;return 0;}
    size_t total=size*count,done=0;
    unsigned char *out=buf;
    if(f->pushed){out[done++]=f->pushback;f->pushed=0;}
    while(done<total){
        if(f->buffering==1 && f->position<f->length){
            size_t n=f->length-f->position;if(n>total-done)n=total-done;
            memcpy(out+done,f->buffer+f->position,n);f->position+=n;done+=n;continue;
        }
        if(f->eof)break;
        ssize_t n=read(f->fd,f->buffering==1?(void *)f->buffer:(void *)(out+done),
                       f->buffering==1?BUFSIZ:total-done);
        if(n<0){if(errno==EINTR)continue;f->error=1;break;}
        if(!n){f->eof=1;break;}
        if(f->buffering==1){f->position=0;f->length=(size_t)n;}
        else done+=(size_t)n;
    }
    return done/size;
}
size_t fwrite(const void *buf,size_t size,size_t count,FILE *f)
{
    if(!size || !count)return 0;
    if(direction(f,1)==EOF)return 0;
    if(count>SIZE_MAX/size){errno=EINVAL;f->error=1;return 0;}
    size_t total=size*count,done=0;
    const unsigned char *in=buf;
    while(done<total){
        if(f->buffering==1){
            if(f->length==BUFSIZ && flush_output(f)==EOF)break;
            size_t n=BUFSIZ-f->length;if(n>total-done)n=total-done;
            memcpy(f->buffer+f->length,in+done,n);f->length+=n;done+=n;
        }else{
            ssize_t n=write(f->fd,in+done,total-done);
            if(n<0 && errno==EINTR)continue;
            if(n<=0){if(!n)errno=EIO;f->error=1;break;}
            done+=(size_t)n;
        }
    }
    return done/size;
}
int fgetc(FILE *f){unsigned char c;return fread(&c,1,1,f)==1?c:EOF;}
int ungetc(int c,FILE *f)
{
    if(c==EOF)return EOF;
    if(direction(f,0)==EOF||f->pushed)return EOF;
    f->pushback=(unsigned char)c;f->pushed=1;f->eof=0;return 0; /* V7 success convention. */
}
int getw(FILE *f){unsigned char bytes[sizeof(int)];if(fread(bytes,1,sizeof(bytes),f)!=sizeof(bytes))return EOF;
    uint32_t word=0;for(unsigned i=0;i<sizeof(bytes);++i)word|=(uint32_t)bytes[i]<<(8*i);return (int)word;}
int putw(int value,FILE *f){unsigned char bytes[sizeof(int)];for(unsigned i=0;i<sizeof(bytes);++i)bytes[i]=(uint32_t)value>>(8*i);
    return fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes)?0:EOF;}
int getc(FILE *f){return fgetc(f);}
int getchar(void){return fgetc(stdin);}
int fputc(int c,FILE *f){unsigned char b=c;return fwrite(&b,1,1,f)==1?b:EOF;}
int putc(int c,FILE *f){return fputc(c,f);}
int putchar(int c){return fputc(c,stdout);}
int fputs(const char *s,FILE *f){size_t n=strlen(s);return fwrite(s,1,n,f)==n?0:EOF;}
int puts(const char *s){return fputs(s,stdout)<0?EOF:fputc('\n',stdout);}
char *fgets(char *s,int size,FILE *f){int i=0,c;if(size<=0)return NULL;while(i<size-1&&(c=fgetc(f))!=EOF){s[i++]=c;if(c=='\n')break;}s[i]=0;return i?s:NULL;}
char *gets(char *s){char *p=s;int c;while((c=getchar())!=EOF&&c!='\n')*p++=c;*p=0;return c==EOF&&p==s?NULL:s;}
int fseek(FILE *f,long offset,int whence)
{
    if(!valid(f))return -1;
    if(flush_output(f)==EOF)return -1;
    if(whence==SEEK_CUR && f->direction==1){
        long unread=(long)(f->length-f->position)+f->pushed;
        if(offset<INT64_MIN+unread){errno=EINVAL;return -1;}
        offset-=unread;
    }
    if(lseek(f->fd,offset,whence)<0)return -1;
    f->position=f->length=0;f->direction=f->eof=f->pushed=0;return 0;
}
long ftell(FILE *f)
{
    if(!valid(f))return -1;
    long position=lseek(f->fd,0,SEEK_CUR);if(position<0)return -1;
    if(f->direction==1)return position-(long)(f->length-f->position)-f->pushed;
    if(f->direction==2){
        if(f->append && f->length){
            /* Inspect EOF without changing the shared descriptor offset. */
            struct stat st;if(fstat(f->fd,&st)<0)return -1;
            position=st.st_size;
        }
        if(position>INT64_MAX-(long)f->length){errno=EINVAL;return -1;}
        position+=(long)f->length;
    }
    return position;
}
void rewind(FILE *f){(void)fseek(f,0,SEEK_SET);f->error=0;}
int feof(FILE *f){return f->eof;}
int ferror(FILE *f){return f->error;}
void clearerr(FILE *f){f->error=f->eof=0;}
struct sink { FILE *file;char *buffer;size_t capacity,count;int failed; };
static void emit(struct sink *s,char c){if(s->file){if(fputc(c,s->file)==EOF)s->failed=1;}else if(s->count+1<s->capacity)s->buffer[s->count]=c;++s->count;}
static size_t floating(char *out, double value, char kind, int precision,int alternate)
{
    size_t n = 0;
    if (!isfinite(value)) {
        const char *s = isnan(value) ? "nan" : "inf";
        memcpy(out, s, 3); return 3;
    }
    if (precision < 0) precision = 6;
    int point, negative, general = kind == 'g' || kind == 'G';
    if (general && !precision) precision = 1;
    char *digits = ecvt(value, general ? precision : precision + 1, &point, &negative);
    int exponential = kind == 'e' || kind == 'E' || (general && (point <= -4 || point > precision));
    if (exponential) {
        int decimals = general ? precision - 1 : precision;
        size_t length = strlen(digits);
        out[n++] = length ? digits[0] : '0';
        if (decimals||alternate) out[n++] = '.';
        for (int i = 1; i <= decimals; ++i) out[n++] = (size_t)i < length ? digits[i] : '0';
        if (general&&!alternate) { while (n && out[n - 1] == '0') --n; if (n && out[n - 1] == '.') --n; }
        out[n++] = kind == 'E' || kind == 'G' ? 'E' : 'e';
        int exponent = value == 0 ? 0 : point - 1;
        out[n++] = exponent < 0 ? '-' : '+';
        if (exponent < 0) exponent = -exponent;
        if (exponent >= 100) out[n++] = '0' + exponent / 100;
        out[n++] = '0' + (exponent / 10) % 10;
        out[n++] = '0' + exponent % 10;
    } else {
        int decimals = general ? precision - point : precision;
        if (decimals < 0) decimals = 0;
        if (!general) digits = fcvt(value, decimals, &point, &negative);
        size_t length = strlen(digits);
        if (point <= 0) out[n++] = '0';
        else for (int i = 0; i < point; ++i) out[n++] = (size_t)i < length ? digits[i] : '0';
        if (decimals||alternate) out[n++] = '.';
        for (int i = 0; i < decimals; ++i) {
            int index = point + i;
            out[n++] = index >= 0 && (size_t)index < length ? digits[index] : '0';
        }
        if (general && decimals&&!alternate) { while (n && out[n - 1] == '0') --n; if (n && out[n - 1] == '.') --n; }
    }
    return n;
}
static int format(struct sink *s,const char *fmt,va_list ap)
{
    va_list arguments;
    va_copy(arguments,ap);
    unsigned remote_switches=0;
    while(*fmt){if(*fmt!='%'){emit(s,*fmt++);continue;}++fmt;
        int left=0,zero=0,width=0,precision=-1,wide=0,plus=0,space=0,alternate=0;
        for(;;){if(*fmt=='-')left=1;else if(*fmt=='0')zero=1;else if(*fmt=='+')plus=1;
            else if(*fmt==' ')space=1;else if(*fmt=='#')alternate=1;else { break; }
            ++fmt;
        }
        if(*fmt=='*'){width=va_arg(arguments,int);++fmt;if(width<0){if(width==INT32_MIN){errno=EINVAL;s->failed=1;break;}left=1;width=-width;}}
        while(*fmt>='0'&&*fmt<='9'){if(width<4096)width=width*10+*fmt-'0';++fmt;}
        if(*fmt=='.'){precision=0;++fmt;if(*fmt=='*'){precision=va_arg(arguments,int);++fmt;if(precision<0)precision=-1;}
            else while(*fmt>='0'&&*fmt<='9'){if(precision<4096)precision=precision*10+*fmt-'0';++fmt;}}
        if(*fmt=='l'){wide=1;++fmt;}
        char kind=*fmt;if(!kind)break;++fmt;
        if(kind=='r'){
            struct nx_printf_remote *remote=va_arg(arguments,struct nx_printf_remote *);
            if(!remote||!remote->format||remote_switches++==16){errno=EINVAL;s->failed=1;break;}
            /* V7 remote replaces the remaining format and argument stream.
             * Copy the native list so packets can be reused without consuming it. */
            va_end(arguments);
            va_copy(arguments,remote->arguments);
            fmt=remote->format;
            continue;
        }
        char digits[768],*p=digits+sizeof(digits);const char *text;size_t n,zeros=0;char sign=0,prefix[2];size_t prefixes=0;
        if(kind=='s'){text=va_arg(arguments,const char *);if(!text)text="(null)";n=0;while(text[n]&&(precision<0||n<(size_t)precision))++n;}
        else if(kind=='c'){digits[0]=va_arg(arguments,int);text=digits;n=1;}
        else if(kind=='%'){digits[0]='%';text=digits;n=1;}
        else if(kind=='d'||kind=='i'||kind=='D'||kind=='u'||kind=='U'||kind=='o'||kind=='O'||kind=='x'||kind=='X'){
            int signed_value=kind=='d'||kind=='i'||kind=='D';unsigned base=kind=='o'||kind=='O'?8:kind=='x'||kind=='X'?16:10;
            uint64_t value;
            if(signed_value){int64_t v=wide||kind=='D'?va_arg(arguments,long):va_arg(arguments,int);if(v<0){sign='-';value=0-(uint64_t)v;}else {value=v;sign=plus?'+':space?' ':0;}}
            else value=wide||kind=='O'||kind=='U'||kind=='X'?va_arg(arguments,unsigned long):va_arg(arguments,unsigned int);
            uint64_t original=value;
            do{unsigned d=value%base;*--p=d<10?'0'+d:'a'+d-10;value/=base;}while(value);
            text=p;n=digits+sizeof(digits)-p;
            if(!original&&precision==0)n=0;
            if(precision>=0){zero=0;if((size_t)precision>n)zeros=(size_t)precision-n;}
            if(alternate&&base==8&&!zeros&&(!n||*text!='0'))zeros=1;
            if(alternate&&base==16&&original){prefix[0]='0';prefix[1]='x';prefixes=2;}
        }else if(kind=='f'||kind=='e'||kind=='E'||kind=='g'||kind=='G'){
            if (precision > 63) { errno=EINVAL;s->failed=1;break; }
            double value=va_arg(arguments,double);
            if(signbit(value)){sign='-';value=-value;}else sign=plus?'+':space?' ':0;
            n=floating(digits,value,kind,precision,alternate);text=digits;
        }else{errno=EINVAL;s->failed=1;break;}
        size_t length=n+zeros+prefixes+(sign!=0);
        size_t pad=(size_t)width>length?(size_t)width-length:0;
        if(!left&&!zero)while(pad){emit(s,' ');--pad;}
        if(sign)emit(s,sign);
        for(size_t i=0;i<prefixes;++i)emit(s,prefix[i]);
        if(!left&&zero)while(pad){emit(s,'0');--pad;}
        while(zeros){emit(s,'0');--zeros;}
        for(size_t i=0;i<n;++i)emit(s,text[i]);
        if(left)while(pad){emit(s,' ');--pad;}
    }
    if(!s->file&&s->capacity)s->buffer[s->count<s->capacity?s->count:s->capacity-1]=0;
    va_end(arguments);
    return s->failed||s->count>INT32_MAX?-1:(int)s->count;
}
int vfprintf(FILE *f,const char *fmt,va_list ap){struct sink s={.file=f};return format(&s,fmt,ap);}
int fprintf(FILE *f,const char *fmt,...){va_list ap;va_start(ap,fmt);int n=vfprintf(f,fmt,ap);va_end(ap);return n;}
int printf(const char *fmt,...){va_list ap;va_start(ap,fmt);int n=vfprintf(stdout,fmt,ap);va_end(ap);return n;}
int vsnprintf(char *b,size_t cap,const char *fmt,va_list ap){struct sink s={.buffer=b,.capacity=cap};return format(&s,fmt,ap);}
int snprintf(char *b,size_t cap,const char *fmt,...){va_list ap;va_start(ap,fmt);int n=vsnprintf(b,cap,fmt,ap);va_end(ap);return n;}
int sprintf(char *b,const char *fmt,...){va_list ap;va_start(ap,fmt);int n=vsnprintf(b,SIZE_MAX,fmt,ap);va_end(ap);return n;}
void perror(const char *s){int error=errno;if(s&&*s)fprintf(stderr,"%s: ",s);fprintf(stderr,"error %d\n",error);errno=error;}

void nx_printf_remote_init(struct nx_printf_remote *remote,const char *fmt,va_list ap)
{ remote->format=fmt;va_copy(remote->arguments,ap); }
void nx_printf_remote_end(struct nx_printf_remote *remote)
{ va_end(remote->arguments);remote->format=NULL; }
