/* V7 stdio/getpass.c with checked terminal setup and cleanup. */
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sgtty.h>
#include <errno.h>

char *getpass(const char *prompt)
{
    static char password[9];
    if(!prompt){errno=EINVAL;return NULL;}
    FILE *input=fopen("/dev/tty","r");
    int owned=input!=NULL;
    if(owned)setbuf(input,NULL);else input=stdin;
    int fd=fileno(input),error=0,changed=0;
    struct sgttyb saved,quiet;
    sighandler_t previous=signal(SIGINT,SIG_IGN);
    if(previous==SIG_ERR){error=errno;goto close_input;}
    if(gtty(fd,&saved)<0){error=errno;goto restore_signal;}
    quiet=saved;quiet.sg_flags&=~ECHO;
    if(stty(fd,&quiet)<0){error=errno;goto restore_signal;}
    changed=1;
    /* The prompt is text, not a caller-controlled printf format. */
    if(fputs(prompt,stderr)<0||fflush(stderr)<0){error=errno;goto restore_terminal;}
    size_t used=0;int c;
    while((c=getc(input))!=EOF&&c!='\n')if(used<8)password[used++]=(char)c;
    password[used]=0;
    if(c==EOF&&ferror(input))error=errno;
    if(fputs("\n",stderr)<0||fflush(stderr)<0){if(!error)error=errno;}
restore_terminal:
    if(changed&&stty(fd,&saved)<0&&!error)error=errno;
restore_signal:
    if(signal(SIGINT,previous)==SIG_ERR&&!error)error=errno;
close_input:
    if(owned&&fclose(input)<0&&!error)error=errno;
    if(error){password[0]=0;errno=error;return NULL;}
    return password;
}
