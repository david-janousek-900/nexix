#ifndef NEXIX_TERMCAP_H
#define NEXIX_TERMCAP_H
#define TCBUFSIZE 2048
/* bp requires TCBUFSIZE bytes. String areas are caller-sized; allow at least
 * TCBUFSIZE for one complete set of distinct string capabilities. */
int tgetent(char *,const char *);
int tgetnum(char *);
int tgetflag(char *);
char *tgetstr(char *,char **);
#endif
