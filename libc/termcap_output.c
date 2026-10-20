/* Bounded native equivalents of the BSD tgoto/tputs contracts. */
#include "include/termcap.h"
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
char *UP,*BC,PC;
short ospeed;
static int append(char *out,size_t *used,size_t capacity,const char *text)
{
    size_t n=strlen(text);if(n>=capacity-*used)return -1;
    memcpy(out+*used,text,n);*used+=n;out[*used]=0;return 0;
}
char *tgoto(char *format,int column,int row)
{
    static char output[256];char correction[128]={0};size_t used=0,extra=0;
    int oncol=0;int64_t values[2]={row,column},value=row;output[0]=0;
    if(!format || row<0 || column<0){errno=EINVAL;return "OOPS";}
    while(*format){if(value<0 || value>INT_MAX)goto range;char text[32]={0};char op=*format++;
        if(op!='%'){text[0]=op;goto emit;}
        op=*format++;if(!op)goto invalid;
        switch(op){
        case 'r':oncol=1;value=values[1];continue;
        case 'i':++values[0];++values[1];++value;continue;
        case 'n':values[0]^=0140;values[1]^=0140;value=values[oncol];continue;
        case 'B':value=(value/10)*16+value%10;continue;
        case 'D':value-=2*(value%16);continue;
        case '>':{
            if(!format[0] || !format[1])goto invalid;
            unsigned a=(unsigned char)*format++,b=(unsigned char)*format++;
            if(value>a)value+=b;
            continue;
        }
        case '%':text[0]='%';break;
        case 'd':case '2':case '3':{
            if(value<0 || value>INT_MAX)goto range;
            char reversed[24];size_t n=0;uint64_t number=(uint64_t)value;
            do{reversed[n++]=(char)('0'+number%10);number/=10;}while(number);
            unsigned width=op=='2'?2:op=='3'?3:1;
            while(n<width)reversed[n++]='0';
            for(size_t i=0;i<n;++i)text[i]=reversed[n-i-1];
            oncol=1-oncol;value=values[oncol];break;
        }
        case '+':if(!*format)goto invalid;value+=(unsigned char)*format++;
            /* fall through */
        case '.':
            if(value<0 || value>255)goto range;
            if(value==0 || value==4 || value==10){
                const char *back=oncol?(BC?BC:"\b"):UP;
                if(!back)goto invalid;
                if(append(correction,&extra,sizeof(correction),back))goto overflow;
                ++value;
            }
            text[0]=(char)value;oncol=1-oncol;value=values[oncol];break;
        default:goto invalid;
        }
emit: if(append(output,&used,sizeof(output),text))goto overflow;
    }
    if(append(output,&used,sizeof(output),correction))goto overflow;
    return output;
overflow:errno=E2BIG;return "OOPS";
range:errno=ERANGE;return "OOPS";
invalid:errno=EINVAL;return "OOPS";
}
int tputs(const char *text,int lines,int (*out)(int))
{
    static const unsigned times[]={0,2000,1333,909,743,666,500,333,166,83,55,41,20,10,5};
    if(!text)return 1;
    if(!out || lines<0){errno=EINVAL;return -1;}
    uint64_t delay=0;
    while(*text>='0' && *text<='9'){
        if(delay>UINT32_MAX/10){errno=ERANGE;return -1;}
        delay=delay*10+(unsigned)(*text++-'0');
    }
    delay*=10;
    if(*text=='.'){
        ++text;if(*text>='0' && *text<='9')delay+=(unsigned)(*text-'0');
        while(*text>='0' && *text<='9')++text;
    }
    if(*text=='*'){++text;if(lines && delay>UINT64_MAX/(unsigned)lines){errno=ERANGE;return -1;}delay*=lines;}
    uint64_t pads=0;
    if(ospeed>0 && (unsigned)ospeed<sizeof(times)/sizeof(times[0]))pads=(delay+times[ospeed]/2)/times[ospeed];
    if(pads>TCBUFSIZE){errno=ERANGE;return -1;}
    while(*text)if(out((unsigned char)*text++)<0)return -1;
    while(pads--)if(out((unsigned char)PC)<0)return -1;
    return 1;
}
