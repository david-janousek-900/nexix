/* Native variadic replacement for V7 stdio/{scanf,doscan}.c. Uses the V7
 * conversion families with bounded arithmetic and byte-safe scan sets. */
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
struct input { FILE *file; const unsigned char *text; size_t position, consumed; };
static int take(struct input *in)
{
    int c=in->file?getc(in->file):in->text[in->position];
    if(!in->file){if(!c)return EOF;++in->position;}
    if(c!=EOF)++in->consumed;
    return c;
}
static void back(struct input *in,int c)
{
    if(c==EOF)return;
    if(in->file)(void)ungetc(c,in->file);else --in->position;
    --in->consumed;
}
static void whitespace(struct input *in)
{ int c;do{c=take(in);}while(isspace(c));back(in,c); }
static int digit_value(int c)
{
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}
static int bounded_take(struct input *in,size_t *width)
{ if(!*width)return EOF;int c=take(in);if(c!=EOF)--*width;return c; }
/* Return one for a match, zero for a mismatch, and -1 for input failure. */
static int integer(struct input *in,size_t width,int base,uint64_t *value,int *negative)
{
    int c=bounded_take(in,&width),digits=0;
    *value=0;*negative=0;
    if(c==EOF)return -1;
    if(c=='+'||c=='-'){*negative=c=='-';c=bounded_take(in,&width);}
    if(c=='0'){
        digits=1;c=bounded_take(in,&width);
        if((base==0||base==16)&&(c=='x'||c=='X')){base=16;digits=0;c=bounded_take(in,&width);}
        else if(base==0)base=8;
    }
    if(!base)base=10;
    int digit;
    while(c!=EOF&&(digit=digit_value(c))>=0&&digit<base){
        if(*value>(UINT64_MAX-(unsigned)digit)/(unsigned)base){*value=UINT64_MAX;errno=ERANGE;}
        else *value=*value*(unsigned)base+(unsigned)digit;
        digits=1;c=bounded_take(in,&width);
    }
    back(in,c);return digits?1:0;
}
static int floating_input(struct input *in,size_t width,double *result)
{
    int c=bounded_take(in,&width),negative=0,digits=0,kept=0,fraction=0;
    int exponent=0;
    long double value=0;
    if(c==EOF)return -1;
    if(c=='+'||c=='-'){negative=c=='-';c=bounded_take(in,&width);}
    while(c!=EOF&&(isdigit(c)||(!fraction&&c=='.'))){
        if(c=='.')fraction=1;
        else {
            digits=1;
            if(kept<19){value=value*10+(c-'0');if(value!=0)++kept;if(fraction&&exponent>-10000)--exponent;}
            else if(!fraction&&exponent<10000)++exponent;
        }
        c=bounded_take(in,&width);
    }
    if(digits&&(c=='e'||c=='E')){
        int minus=0,power=0,edigits=0;
        c=bounded_take(in,&width);
        if(c=='+'||c=='-'){minus=c=='-';c=bounded_take(in,&width);}
        while(isdigit(c)){edigits=1;if(power<10000)power=power*10+c-'0';c=bounded_take(in,&width);}
        if(!edigits){back(in,c);return 0;}
        exponent+=minus?-power:power;
    }
    back(in,c);
    if(!digits)return 0;
    if(value!=0){
        if(exponent>5000)value=INFINITY;
        else if(exponent< -5000)value=0;
        else {while(exponent>0){value*=10;--exponent;}while(exponent<0){value/=10;++exponent;}}
    }
    *result=(double)value;
    if(!isfinite(*result)||(kept&&(*result==0||*result<0x1p-1022)))errno=ERANGE;
    if(negative)*result=-*result;
    return 1;
}
static int scan(struct input *in,const char *format,va_list arguments)
{
    int matches=0;
    while(*format){
        if(isspace((unsigned char)*format)){while(isspace((unsigned char)*format))++format;whitespace(in);continue;}
        if(*format!='%'||format[1]=='%'){
            int expected=*format++;if(expected=='%')++format;
            int c=take(in);
            if(c!=expected){back(in,c);return c==EOF&&!matches?EOF:matches;}
            continue;
        }
        ++format;
        int suppress=*format=='*';if(suppress)++format;
        size_t width=SIZE_MAX;
        if(isdigit((unsigned char)*format)){
            width=0;while(isdigit((unsigned char)*format)){
                unsigned d=*format++-'0';width=width>(SIZE_MAX-d)/10?SIZE_MAX:width*10+d;
            }
            if(!width){errno=EINVAL;return matches;}
        }
        int size=0; /* -1 short, 0 int/float, 1 long/double */
        if(*format=='h'){size=-1;++format;}else if(*format=='l'){size=1;++format;}
        int kind=(unsigned char)*format;
        if(!kind){errno=EINVAL;return matches;}
        ++format;
        if(isupper(kind)){kind=tolower(kind);size=1;}
        if(kind=='n'){
            if(!suppress){if(size==1)*va_arg(arguments,long *)=(long)in->consumed;
                else if(size<0)*va_arg(arguments,short *)=(short)in->consumed;
                else *va_arg(arguments,int *)=(int)in->consumed;}
            continue;
        }
        if(kind!='c'&&kind!='[')whitespace(in);
        if(kind=='c'||kind=='s'||kind=='['){
            unsigned char set[256]={0};int invert=0;
            if(kind=='['){
                if(*format=='^'){invert=1;++format;}
                if(*format==']')set[(unsigned char)*format++]=1;
                while(*format&&*format!=']')set[(unsigned char)*format++]=1;
                if(*format!=']'){errno=EINVAL;return matches;}++format;
            }
            if(kind=='c'&&width==SIZE_MAX)width=1;
            char *out=suppress?NULL:va_arg(arguments,char *);
            size_t count=0;int c=EOF;
            while(count<width){
                c=take(in);
                if(c==EOF)break;
                if((kind=='s'&&isspace(c))||(kind=='['&&!(set[c]^invert))){back(in,c);break;}
                if(out)out[count]=(char)c;
                ++count;
            }
            if(!count)return c==EOF&&!matches?EOF:matches;
            if(out){if(kind!='c')out[count]=0;++matches;}
            continue;
        }
        if(kind=='d'||kind=='i'||kind=='u'||kind=='o'||kind=='x'){
            uint64_t value;int negative;
            int status=integer(in,width,kind=='o'?8:kind=='x'?16:kind=='i'?0:10,&value,&negative);
            if(status!=1)return status<0&&!matches?EOF:matches;
            if(!suppress){
                int signed_value=kind=='d'||kind=='i';
                uint64_t limit=size==1?(signed_value?INT64_MAX:UINT64_MAX):size<0?(signed_value?INT16_MAX:UINT16_MAX):(signed_value?INT32_MAX:UINT32_MAX);
                if(signed_value&&negative)++limit;
                if(value>limit){value=limit;errno=ERANGE;}
                uint64_t bits=negative?0-value:value;
                if(signed_value){if(size==1)*va_arg(arguments,long *)=(long)bits;else if(size<0)*va_arg(arguments,short *)=(short)bits;else *va_arg(arguments,int *)=(int)bits;}
                else {if(size==1)*va_arg(arguments,unsigned long *)=bits;else if(size<0)*va_arg(arguments,unsigned short *)=bits;else *va_arg(arguments,unsigned int *)=bits;}
                ++matches;
            }
            continue;
        }
        if(kind=='e'||kind=='f'||kind=='g'){
            double value;int status=floating_input(in,width,&value);
            if(status!=1)return status<0&&!matches?EOF:matches;
            if(!suppress){if(size==1)*va_arg(arguments,double *)=value;else {
                float narrow=(float)value;
                if(isfinite(value)&&(!isfinite(narrow)||(value!=0&&narrow==0)))errno=ERANGE;
                *va_arg(arguments,float *)=narrow;
            }++matches;}
            continue;
        }
        errno=EINVAL;return matches;
    }
    return matches;
}
int vfscanf(FILE *file,const char *format,va_list arguments)
{ struct input in={.file=file};return scan(&in,format,arguments); }
int vsscanf(const char *text,const char *format,va_list arguments)
{ struct input in={.text=(const unsigned char *)text};return scan(&in,format,arguments); }
int fscanf(FILE *file,const char *format,...)
{ va_list ap;va_start(ap,format);int n=vfscanf(file,format,ap);va_end(ap);return n; }
int sscanf(const char *text,const char *format,...)
{ va_list ap;va_start(ap,format);int n=vsscanf(text,format,ap);va_end(ap);return n; }
int scanf(const char *format,...)
{ va_list ap;va_start(ap,format);int n=vfscanf(stdin,format,ap);va_end(ap);return n; }
int vscanf(const char *format,va_list arguments)
{ return vfscanf(stdin,format,arguments); }
