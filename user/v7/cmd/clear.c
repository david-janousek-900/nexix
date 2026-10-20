/* Native termcap client; the terminal description owns the escape sequence. */
#include <termcap.h>
#include <stdlib.h>
#include <stdio.h>
static int emit(int c) { return putchar(c); }
int main(int argc, char **argv)
{
    char entry[TCBUFSIZE], strings[TCBUFSIZE], *area=strings;
    char *term=getenv("TERM"), *sequence;
    (void)argv;
    if(argc!=1) { fputs("usage: clear\n",stderr); return 2; }
    if(!term || !*term) { fputs("clear: TERM is not set\n",stderr); return 1; }
    if(tgetent(entry,term)!=1) { fputs("clear: terminal description unavailable\n",stderr); return 1; }
    sequence=tgetstr("cl",&area);
    if(!sequence || !*sequence) { fputs("clear: terminal cannot clear the screen\n",stderr); return 1; }
    /* No padding is needed for the native console or modern serial emulators. */
    ospeed=0;
    if(tputs(sequence,1,emit)<0 || fflush(stdout)==EOF) {
        fputs("clear: output error\n",stderr); return 1;
    }
    return 0;
}
