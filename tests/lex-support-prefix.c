#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#define YYLMAX 200
#define YYNEWLINE 10
struct yywork {int verify,advance;};
struct yysvf {struct yywork *yystoff;struct yysvf *yyother;int *yystops;};
static struct yywork storage[1024];
#define yycrank (storage+16)
static struct yywork *yytop=storage+1023;
static struct yysvf yysvec[4];
#define YYLERR yysvec
static struct yysvf *yybgin=yysvec+1;
struct yysvf *yyestate;
static char yymatch[256],yyextra[2];
static int yymorfg,yyleng;
extern char *yysptr,yysbuf[];
static int next_byte,available,read_calls;
static int input_byte(void){assert(++read_calls<20);if(yysptr>yysbuf)return (unsigned char)*--yysptr;if(available){available=0;return next_byte;}return 0;}

#define input() input_byte()
#define unput(c) (*yysptr++=(char)(c))
#define output(c) ((void)(c))
