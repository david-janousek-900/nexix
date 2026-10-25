#ifndef NX_NSLOOKUP_RES_H
#define NX_NSLOOKUP_RES_H
#include <stdio.h>
void SetDefaultServer(const char *,int);
void SetOption(const char *);
void PrintHelp(void);
void LookupHost(const char *,int);
void LookupHostWithServer(const char *,int);
void ListHosts(const char *,int);
void ListHostsByType(const char *,int);
void Finger(const char *,int);
void ViewList(const char *);
int nslookup_input(FILE *);
void nslookup_lexer_reset(void);
int yylex(void);
int yywrap(void);
#endif
