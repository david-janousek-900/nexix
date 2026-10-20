/* Nexix pager: retained manual formatting, modern terminal navigation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <termcap.h>
#include <sys/ioctl.h>
#include <errno.h>
#define LIMIT (4U*1024U*1024U)
static struct termios saved;
static int tty=-1,changed;
static volatile sig_atomic_t interrupted;
static char *clear_screen,*cursor;
static int emit(int c){return putchar(c);}
static void restore(void){if(changed){tcsetattr(tty,TCSANOW,&saved);changed=0;}}
static void stop(int n){interrupted=n;}
static int key(void){unsigned char c;ssize_t n;do{n=read(tty,&c,1);}while(n<0 && errno==EINTR && !interrupted);if(n==1 && c==saved.c_cc[VINTR] && c!=_POSIX_VDISABLE){interrupted=SIGINT;return -1;}
if(n==1 && c==saved.c_cc[VQUIT] && c!=_POSIX_VDISABLE){interrupted=SIGQUIT;return -1;}
return n==1?c:-1;}
static void position(int row){tputs(tgoto(cursor,0,row),1,emit);}
static int stream(FILE *input){int c;while((c=getc(input))!=EOF)if(putchar(c)==EOF)return 1;return ferror(input) || fflush(stdout)==EOF;}
static int contains(const char *s,const char *p){size_t n=strlen(p);for(;*s;s++)if(!strncmp(s,p,n))return 1;return 0;}
static int usage(void){fputs("usage: more [file]\n",stderr);return 2;}
int main(int argc,char **argv)
{
    FILE *input=stdin;char entry[TCBUFSIZE],strings[TCBUFSIZE],*area=strings,*term=getenv("TERM");
    char *text=NULL,**lines=NULL,search[160]="",message[180]="";size_t used=0,capacity=0,count=0,linecap=0,top=0,i,start;
    int rows=24,columns=80,c,direction=1,result=0;char status[400];struct winsize ws;
    if(argc>2 || (argc==2 && argv[1][0]=='-'))return usage();
    if(argc==2 && !(input=fopen(argv[1],"r"))){perror("more");return 1;}
    if(!isatty(1) || !term || tgetent(entry,term)!=1 || !(clear_screen=tgetstr("cl",&area)) || !(cursor=tgetstr("cm",&area))){
        result=stream(input);if(input!=stdin)fclose(input);return result;
    }
    {char *device=ttyname(1);tty=open(device?device:"/dev/tty",O_RDWR);}
    if(tty<0 || tcgetattr(tty,&saved)){result=stream(input);goto done;}
    if(!ioctl(1,TIOCGWINSZ,&ws)){if(ws.ws_row>=3 && ws.ws_row<=100)rows=ws.ws_row;if(ws.ws_col>=10 && ws.ws_col<=240)columns=ws.ws_col;}
    /* Read before changing the terminal, so failed producers cannot leave raw mode. */
    while((c=getc(input))!=EOF){
        if(used+2>=capacity){size_t next=capacity?capacity*2:8192;char *p;
            if(next>LIMIT){fputs("more: input exceeds 4 MiB\n",stderr);result=1;goto done;}
            p=realloc(text,next);if(!p){perror("more");result=1;goto done;}text=p;capacity=next;}
        text[used++]=c;
    }
    if(ferror(input)){fputs("more: input error\n",stderr);result=1;goto done;}
    if(!used)goto done;
    /* Normalize nroff overstrikes, tabs and carriage motion to printable rows.
     * Output never interprets escape sequences embedded in a document. */
    {size_t in=0,out=0,outcap=used*9+3;char row[241];int length=0,x=0;
     if(outcap>LIMIT)outcap=LIMIT;
     char *decoded=malloc(outcap);if(!decoded){perror("more");result=1;goto done;}
     while(in<=used){c=in<used?(unsigned char)text[in++]:-1;
        if(c=='\n'||c==-1){if(out+(size_t)length+2>=outcap){free(decoded);fputs("more: decoded input exceeds limit\n",stderr);result=1;goto done;}for(int j=0;j<length;j++)decoded[out++]=row[j];decoded[out++]='\n';length=x=0;if(c==-1)break;continue;}
        if(c=='\b'){if(x)x--;continue;}if(c=='\r'){x=0;continue;}
        int spaces=c=='\t'?8-x%8:1;
        if(c!='\t' && (c<32 || c>126))continue;
        while(spaces--){if(x>=columns){if(out+(size_t)length+2>=outcap){free(decoded);fputs("more: decoded input exceeds limit\n",stderr);result=1;goto done;}for(int j=0;j<length;j++)decoded[out++]=row[j];decoded[out++]='\n';length=x=0;}
            while(length<=x)row[length++]=' ';
            if(c!='_' || row[x]==' ')row[x]=(c=='\t'?' ':c);
            x++;
        }
     }
     free(text);text=decoded;used=out;text[used]=0;
    }
    start=0;for(i=0;i<used;i++)if(text[i]=='\n'){
        text[i]=0;
        if(count==linecap){size_t next=linecap?linecap*2:256;char **p=realloc(lines,next*sizeof(*lines));if(!p){perror("more");result=1;goto done;}lines=p;linecap=next;}
        lines[count++]=text+start;start=i+1;
    }
    if(count && !*lines[count-1])count--; /* No synthetic final row. */
    struct termios raw=saved;raw.c_lflag&=~(ICANON|ECHO|ISIG);raw.c_cc[VMIN]=1;raw.c_cc[VTIME]=0;
    signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGHUP,stop);signal(SIGQUIT,stop);signal(SIGPIPE,stop);
    if(tcsetattr(tty,TCSANOW,&raw)){perror("more");result=1;goto done;}changed=1;
    while(!interrupted){
        tputs(clear_screen,1,emit);
        for(i=0;i<(size_t)(rows-1) && top+i<count;i++){position(i);fputs(lines[top+i],stdout);}
        position(rows-1);
        snprintf(status,sizeof(status),"--More-- %lu/%lu%s  Space/b j/k g/G /? n/N q%s%s",(unsigned long)(count?top+1:0),(unsigned long)count,top+(rows-1)>=count?" (END)":"",*message?"  ":"",message);
        printf("%.*s",columns-1,status);
        *message=0;if(fflush(stdout)==EOF){result=1;break;}
        c=key();if(c<0 || c=='q' || c=='Q')break;
        if(c==27){c=key();if(c=='['){c=key();if(c=='A')c='k';else if(c=='B')c='j';else if(c=='H')c='g';else if(c=='F')c='G';else if(c=='5'||c=='6'){int page=c;if(key()=='~')c=page=='5'?'b':' ';}else continue;}else continue;}
        if(c==' '||c=='f'||c==6)top+=rows-1;
        else if(c=='b'||c==2)top=top>=(size_t)(rows-1)?top-(rows-1):0;
        else if(c=='j'||c=='\n'||c=='\r'||c==14)top++;
        else if(c=='k'||c==16){if(top)top--;}
        else if(c=='d'||c==4)top+=(rows-1)/2;
        else if(c=='u'||c==21)top=top>=(size_t)((rows-1)/2)?top-(rows-1)/2:0;
        else if(c=='g')top=0;
        else if(c=='G')top=count>(size_t)(rows-1)?count-(rows-1):0;
        else if(c=='h'){strcpy(message,"Arrows: lines; PgUp/PgDn: pages; g/G: start/end; /?: search; q: quit");continue;}
        else if(c=='/'||c=='?'||c=='n'||c=='N'){
            int way=direction;
            if(c=='/'||c=='?'){
                way=c=='/'?1:-1;direction=way;char pattern[160];size_t n=0;int cancelled=0;
                position(rows-1);printf("%c",c);fflush(stdout);
                while((c=key())>=0 && c!='\r' && c!='\n'){
                    if(c==27){cancelled=1;break;}
                    if(c==127||c==8){if(n){n--;fputs("\b \b",stdout);}}
                    else if(c>=32 && c<=126 && n+1<sizeof(pattern)){pattern[n++]=c;putchar(c);}
                    fflush(stdout);
                }
                if(c<0)break;
                if(cancelled)continue;
                pattern[n]=0;if(n)strcpy(search,pattern);
            }else if(c=='N')way=-way;
            if(!*search){strcpy(message,"No search pattern");continue;}
            size_t at=top;int found=0;
            while(way>0?at+1<count:at>0){at=way>0?at+1:at-1;if(contains(lines[at],search)){top=at;found=1;break;}}
            if(!found)strcpy(message,"Pattern not found");
        }
        if(count && top>=count)top=count-1;
    }
    restore();tputs(clear_screen,1,emit);fflush(stdout);if(interrupted)result=128+interrupted;
done:
    restore();if(tty>=0)close(tty);if(input!=stdin)fclose(input);free(lines);free(text);return result;
}
