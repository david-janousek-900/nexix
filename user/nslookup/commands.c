# include "stdio.h"
# define U(x) x
# define NLSTATE yyprevious=YYNEWLINE
# define BEGIN yybgin = yysvec + 1 +
# define INITIAL 0
# define YYLERR yysvec
# define YYSTATE (yyestate-yysvec-1)
# define YYOPTIM 1
# define YYLMAX 200
# define output(c) putc(c,yyout)
# define input() (((yytchar=yysptr>yysbuf?U(*--yysptr):getc(yyin))==10?(yylineno++,yytchar):yytchar)==EOF?0:yytchar)
# define unput(c) {yytchar= (c);if(yytchar=='\n')yylineno--;*yysptr++=yytchar;}
# define yymore() (yymorfg=1)
# define ECHO fprintf(yyout, "%s",yytext)
# define REJECT { nstr = yyreject(); goto yyfussy;}
int yyleng; extern char yytext[];
int yymorfg;
extern char *yysptr, yysbuf[];
int yytchar;
FILE *yyin, *yyout;
extern int yylineno;
struct yysvf { 
	struct yywork *yystoff;
	struct yysvf *yyother;
	int *yystops;};
struct yysvf *yyestate;
extern struct yysvf yysvec[], *yybgin;
#undef YYLMAX
#define YYLMAX 2048

/*
 * ++Copyright++ 1985
 * -
 * Copyright (c) 1985
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */



/*
 *******************************************************************************
 *
 *  commands.l
 *
 *	Andrew Cherenson	CS298-26  Fall 1985
 *
 *	Lex input file for the nslookup program command interpreter.
 *	When a sequence is recognized, the associated action
 *	routine is called. The action routine may need to
 *	parse the string for additional information.
 *
 *  Recognized commands: (identifiers are shown in uppercase)
 *
 *	server NAME	- set default server to NAME, using default server
 *	lserver NAME	- set default server to NAME, using initial server
 *	finger [NAME]	- finger the optional NAME
 *	exit		- exit the program
 *	root		- set default server to the root
 *	ls NAME		- list the domain NAME
 *	view FILE	- sorts and view the file with more
 *	set OPTION	- set an option
 *	help		- print help information
 *	?		- print help information
 *	NAME		- print info about the host/domain NAME
 *			  using default server.
 *	NAME1 NAME2	- as above, but use NAME2 as server
 *
 *
 *   yylex Results:
 *	0		upon end-of-file.
 *	1		after each command.
 *
 *******************************************************************************
 */

#include "res.h"
/* Native stdio retries EINTR; interactive reads must reach the prompt. */
#define getc nslookup_input
extern char rootServerName[];
void PrintHelp(void);

# define YYNEWLINE 10
yylex(){
int nstr; extern int yyprevious;
if(yyin == NULL) yyin = stdin;
if(yyout == NULL) yyout = stdout;
while((nstr = yylook()) >= 0)
yyfussy: switch(nstr){
case 0:
if(yywrap()) return(0); break;
case 1:
{
					    /*
					     * 0 == use current server to find
					     *	    the new one.
					     * 1 == use original server to find
					     *	    the new one.
					     */
					    SetDefaultServer(yytext, 0);
					    return(1);
					}
break;
case 2:
{
					    SetDefaultServer(yytext, 1);
					    return(1);
					}
break;
case 3:
		{
					    return(0);
					}
break;
case 4:
		{
					    SetDefaultServer(rootServerName, 1);
					    return(1);
					}
break;
case 5:
{
					    /*
					     * 2nd arg.
					     *  0 == output to stdout
					     *  1 == output to file
					     */
					    Finger(yytext, 1);
					    return(1);
					}
break;
case 6:
{
					    Finger(yytext, 0);
					    return(1);
					}
break;
case 7:
{
					    ViewList(yytext);
					    return(1);
					}
break;
case 8:
{
					    /*
					     * 2nd arg.
					     *  0 == output to stdout
					     *  1 == output to file
					     */
					    ListHosts(yytext, 1);
					    return(1);
					}
break;
case 9:
{
					    ListHosts(yytext, 0);
					    return(1);
					}
break;
case 10:
{
					    /*
					     * 2nd arg.
					     *  0 == output to stdout
					     *  1 == output to file
					     */
					    ListHostsByType(yytext, 1);
					    return(1);
					}
break;
case 11:
{
					    ListHostsByType(yytext, 0);
					    return(1);
					}
break;
case 12:
	{
					    SetOption(yytext);
					    return(1);
					}
break;
case 13:
		{
					    PrintHelp();
					    return(1);
					}
break;
case 14:
			{
					    void PrintHelp(void);

					    PrintHelp();
					    return(1);
					}
break;
case 15:
{
					    /*
					     * 0 == output to stdout
					     * 1 == output to file
					     */
					    LookupHost(yytext, 1);
					    return(1);
					}
break;
case 16:
	{
					    LookupHost(yytext, 0);
					    return(1);
					}
break;
case 17:
{
					    /*
					     * 0 == output to stdout
					     * 1 == output to file
					     */
					    LookupHostWithServer(yytext, 1);
					    return(1);
					}
break;
case 18:
{
					    LookupHostWithServer(yytext, 0);
					    return(1);
					}
break;
case 19:
			{
					    return(1);
					}
break;
case 20:
				{
					    printf("Unrecognized command: %s",
							yytext);
					    return(1);
					}
break;
case 21:
				{ ; }
break;
case -1:
break;
default:
fprintf(yyout,"bad switch yylook %d",nstr);
} return(0); }
/* end of yylex */

/* Discard lookahead and partial tokens after an input interrupt. */
void nslookup_lexer_reset(void)
{
    extern int yyprevious;
    yysptr=yysbuf;
    yyprevious=10;
    yyleng=0;
    yymorfg=0;
}
int yyvstop[] ={
0,

21,
0,

19,
20,
21,
0,

-16,
0,

-14,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

20,
0,

19,
20,
0,

-16,
0,

16,
20,
0,

14,
20,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

-18,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

-16,
0,

-18,
0,

18,
20,
0,

-15,
0,

-3,
-16,
0,

-16,
0,

-13,
-16,
0,

-9,
-18,
0,

-16,
0,

-4,
-16,
0,

-16,
0,

-16,
0,

-16,
0,

-15,
0,

15,
20,
0,

-3,
-16,
0,

3,
16,
20,
0,

-16,
0,

-13,
-16,
0,

13,
16,
20,
0,

-9,
-18,
0,

9,
18,
20,
0,

-16,
0,

-4,
-16,
0,

4,
16,
20,
0,

-16,
0,

-12,
-18,
0,

-12,
0,

-16,
0,

-17,
0,

-6,
-16,
0,

-16,
0,

-16,
0,

-12,
-18,
0,

12,
18,
20,
0,

-12,
0,

12,
20,
0,

-7,
-18,
0,

-7,
0,

-17,
0,

17,
20,
0,

-6,
-16,
0,

6,
16,
20,
0,

-9,
0,

-11,
0,

-16,
0,

-16,
0,

-7,
-18,
0,

7,
18,
20,
0,

-7,
0,

7,
20,
0,

-6,
-18,
0,

-8,
-17,
0,

-9,
0,

9,
20,
0,

-11,
0,

11,
20,
0,

-16,
0,

-1,
-18,
0,

-6,
-18,
0,

6,
18,
20,
0,

-5,
-15,
0,

-8,
-17,
0,

8,
17,
20,
0,

-11,
0,

-2,
-18,
0,

-1,
-18,
0,

1,
18,
20,
0,

-5,
-15,
0,

5,
15,
20,
0,

-11,
0,

-2,
-18,
0,

2,
18,
20,
0,

-5,
-17,
0,

-8,
0,

-10,
0,

-5,
-17,
0,

5,
17,
20,
0,

-8,
0,

8,
20,
0,

-10,
0,

10,
20,
0,
0};
# define YYTYPE int
struct yywork { YYTYPE verify, advance; } yycrank[] ={
0,0,	0,0,	0,0,	0,0,	
2,4,	0,0,	4,4,	0,0,	
0,0,	0,0,	0,0,	1,3,	
2,5,	2,6,	4,4,	4,16,	
7,18,	7,19,	5,17,	8,8,	
8,20,	9,18,	9,19,	10,18,	
10,19,	18,18,	18,19,	29,41,	
11,18,	11,19,	39,39,	39,40,	
12,18,	12,19,	13,18,	13,19,	
14,18,	14,19,	54,54,	54,55,	
63,76,	15,18,	15,19,	21,18,	
21,19,	2,7,	64,77,	4,4,	
2,4,	7,7,	4,4,	41,41,	
7,7,	53,72,	9,7,	76,76,	
10,7,	9,7,	18,28,	10,7,	
29,42,	11,7,	115,124,	29,42,	
11,7,	12,7,	2,8,	13,7,	
12,7,	14,7,	13,7,	5,8,	
14,7,	125,124,	15,7,	77,77,	
21,7,	15,7,	18,29,	21,7,	
29,41,	82,82,	82,83,	39,53,	
41,42,	128,127,	53,73,	41,42,	
76,92,	53,73,	0,0,	22,18,	
22,19,	86,86,	86,87,	2,7,	
0,0,	4,4,	23,18,	23,19,	
98,98,	98,99,	113,113,	113,114,	
2,9,	2,10,	53,72,	2,11,	
77,93,	5,9,	5,10,	2,12,	
5,11,	24,33,	24,19,	115,125,	
5,12,	2,13,	2,14,	10,22,	
11,23,	2,15,	5,13,	5,14,	
22,7,	117,127,	5,15,	22,7,	
14,26,	25,18,	25,19,	23,7,	
9,21,	26,4,	23,7,	0,0,	
13,25,	15,27,	12,24,	21,30,	
0,0,	26,18,	26,19,	27,18,	
27,19,	0,0,	24,7,	28,39,	
28,40,	24,7,	0,0,	30,18,	
30,19,	31,18,	31,19,	35,18,	
35,19,	32,18,	32,19,	0,0,	
33,4,	0,0,	25,7,	34,18,	
34,19,	25,7,	61,61,	61,62,	
33,33,	33,19,	56,56,	56,57,	
80,80,	80,81,	26,7,	0,0,	
27,7,	26,7,	117,128,	27,7,	
28,28,	36,18,	36,19,	28,28,	
30,7,	0,0,	31,7,	30,7,	
35,7,	31,7,	32,7,	35,7,	
22,31,	32,7,	37,51,	37,19,	
34,7,	23,32,	0,0,	34,7,	
0,0,	33,46,	0,0,	56,28,	
33,47,	24,34,	33,4,	38,18,	
38,19,	42,54,	42,55,	0,0,	
96,96,	96,97,	36,7,	122,122,	
122,123,	36,7,	0,0,	61,75,	
33,4,	33,29,	0,0,	56,29,	
26,4,	80,53,	0,0,	37,7,	
43,56,	43,57,	37,7,	25,35,	
0,0,	44,18,	44,19,	27,38,	
0,0,	45,59,	45,60,	0,0,	
38,7,	0,0,	42,42,	38,7,	
0,0,	42,42,	26,36,	31,44,	
26,37,	46,61,	46,62,	33,4,	
0,0,	72,72,	33,4,	48,18,	
48,19,	0,0,	30,43,	0,0,	
32,45,	43,7,	35,49,	47,4,	
43,7,	96,53,	44,7,	0,0,	
34,48,	44,7,	45,7,	47,4,	
47,16,	45,7,	0,0,	49,66,	
49,67,	50,18,	50,19,	51,51,	
51,19,	0,0,	46,46,	52,71,	
52,19,	46,46,	72,73,	0,0,	
48,7,	72,73,	36,50,	48,7,	
0,0,	58,18,	58,19,	59,59,	
59,60,	65,18,	65,19,	135,135,	
135,136,	90,90,	66,66,	66,67,	
47,4,	68,18,	68,19,	47,4,	
49,7,	91,90,	50,7,	49,7,	
51,69,	50,7,	0,0,	51,70,	
52,7,	38,52,	0,0,	52,7,	
69,80,	69,81,	137,137,	137,138,	
0,0,	44,58,	58,7,	75,90,	
59,28,	58,7,	65,7,	0,0,	
51,29,	65,7,	90,102,	66,28,	
0,0,	90,102,	68,7,	70,82,	
70,83,	68,7,	91,73,	71,71,	
71,19,	91,73,	103,103,	103,104,	
59,29,	73,86,	73,87,	74,88,	
74,89,	69,69,	47,4,	66,29,	
69,69,	78,18,	78,19,	47,63,	
75,73,	111,111,	47,63,	75,73,	
48,65,	0,0,	47,63,	79,95,	
79,19,	50,68,	0,0,	47,63,	
70,70,	84,96,	84,97,	70,70,	
71,84,	47,63,	47,64,	71,85,	
75,91,	0,0,	73,73,	0,0,	
74,7,	73,73,	101,111,	74,7,	
0,0,	65,78,	78,7,	85,98,	
85,99,	78,7,	111,112,	0,0,	
71,29,	111,112,	58,74,	103,115,	
79,7,	88,88,	88,89,	79,7,	
92,103,	92,104,	84,84,	109,109,	
109,110,	84,84,	68,79,	93,105,	
93,106,	94,107,	94,19,	95,95,	
95,19,	100,109,	100,110,	101,112,	
119,119,	119,120,	101,112,	0,0,	
85,85,	102,113,	102,114,	85,85,	
126,126,	126,106,	105,105,	105,106,	
107,107,	107,19,	88,100,	0,0,	
121,131,	92,92,	0,0,	101,111,	
92,92,	108,119,	108,120,	124,124,	
93,93,	0,0,	94,7,	93,93,	
95,108,	94,7,	100,100,	112,122,	
112,123,	100,100,	88,101,	116,126,	
116,106,	127,127,	102,102,	131,131,	
109,121,	102,102,	78,94,	105,116,	
0,0,	107,118,	118,129,	118,130,	
95,29,	121,132,	129,129,	129,130,	
121,132,	119,53,	108,108,	0,0,	
124,133,	108,108,	0,0,	124,133,	
0,0,	126,117,	0,0,	105,117,	
112,112,	107,29,	0,0,	112,112,	
116,116,	121,131,	127,134,	116,116,	
131,132,	127,134,	0,0,	131,132,	
132,135,	132,136,	0,0,	118,118,	
133,137,	133,138,	118,118,	0,0,	
134,139,	134,140,	139,139,	139,140,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	129,53,	
0,0,	0,0,	0,0,	0,0,	
0,0,	132,132,	0,0,	0,0,	
132,132,	133,133,	0,0,	0,0,	
133,133,	134,134,	0,0,	0,0,	
134,134,	0,0,	0,0,	0,0,	
0,0};
struct yysvf yysvec[] ={
0,	0,	0,
yycrank+1,	0,		0,	
yycrank+-3,	0,		0,	
yycrank+0,	0,		yyvstop+1,
yycrank+-5,	0,		0,	
yycrank+-8,	yysvec+2,	0,	
yycrank+0,	0,		yyvstop+3,
yycrank+-7,	yysvec+4,	yyvstop+7,
yycrank+-10,	yysvec+4,	yyvstop+9,
yycrank+-12,	yysvec+4,	yyvstop+11,
yycrank+-14,	yysvec+4,	yyvstop+13,
yycrank+-19,	yysvec+4,	yyvstop+15,
yycrank+-23,	yysvec+4,	yyvstop+17,
yycrank+-25,	yysvec+4,	yyvstop+19,
yycrank+-27,	yysvec+4,	yyvstop+21,
yycrank+-32,	yysvec+4,	yyvstop+23,
yycrank+0,	0,		yyvstop+25,
yycrank+0,	0,		yyvstop+27,
yycrank+-16,	yysvec+4,	yyvstop+30,
yycrank+0,	0,		yyvstop+32,
yycrank+0,	0,		yyvstop+35,
yycrank+-34,	yysvec+4,	yyvstop+38,
yycrank+-82,	yysvec+4,	yyvstop+40,
yycrank+-89,	yysvec+4,	yyvstop+42,
yycrank+-104,	yysvec+4,	yyvstop+44,
yycrank+-120,	yysvec+4,	yyvstop+46,
yycrank+-132,	0,		yyvstop+48,
yycrank+-134,	yysvec+4,	yyvstop+50,
yycrank+-138,	yysvec+4,	yyvstop+52,
yycrank+-18,	yysvec+4,	0,	
yycrank+-142,	yysvec+4,	yyvstop+54,
yycrank+-144,	yysvec+4,	yyvstop+56,
yycrank+-148,	yysvec+4,	yyvstop+58,
yycrank+-159,	0,		yyvstop+60,
yycrank+-154,	yysvec+4,	yyvstop+62,
yycrank+-146,	yysvec+4,	yyvstop+64,
yycrank+-172,	yysvec+4,	yyvstop+66,
yycrank+-185,	yysvec+4,	yyvstop+68,
yycrank+-198,	yysvec+4,	yyvstop+70,
yycrank+-21,	yysvec+4,	yyvstop+72,
yycrank+0,	0,		yyvstop+74,
yycrank+-42,	yysvec+4,	0,	
yycrank+-200,	yysvec+4,	yyvstop+77,
yycrank+-219,	yysvec+4,	yyvstop+79,
yycrank+-224,	yysvec+4,	yyvstop+82,
yycrank+-228,	yysvec+4,	yyvstop+84,
yycrank+-240,	yysvec+4,	yyvstop+87,
yycrank+-262,	0,		0,	
yycrank+-246,	yysvec+4,	yyvstop+90,
yycrank+-266,	yysvec+4,	yyvstop+92,
yycrank+-268,	yysvec+4,	yyvstop+95,
yycrank+-270,	yysvec+4,	yyvstop+97,
yycrank+-274,	yysvec+4,	yyvstop+99,
yycrank+-44,	yysvec+4,	0,	
yycrank+-29,	yysvec+4,	yyvstop+101,
yycrank+0,	0,		yyvstop+103,
yycrank+-161,	yysvec+4,	yyvstop+106,
yycrank+0,	0,		yyvstop+109,
yycrank+-284,	yysvec+4,	yyvstop+113,
yycrank+-286,	yysvec+4,	yyvstop+115,
yycrank+0,	0,		yyvstop+118,
yycrank+-157,	yysvec+4,	yyvstop+122,
yycrank+0,	0,		yyvstop+125,
yycrank+-31,	yysvec+4,	0,	
yycrank+-37,	yysvec+4,	0,	
yycrank+-288,	yysvec+4,	yyvstop+129,
yycrank+-293,	yysvec+4,	yyvstop+131,
yycrank+0,	0,		yyvstop+134,
yycrank+-296,	yysvec+4,	yyvstop+138,
yycrank+-311,	yysvec+4,	yyvstop+140,
yycrank+-330,	yysvec+4,	yyvstop+143,
yycrank+-334,	yysvec+4,	yyvstop+145,
yycrank+-244,	yysvec+4,	0,	
yycrank+-340,	yysvec+4,	yyvstop+147,
yycrank+-342,	yysvec+4,	yyvstop+149,
yycrank+-318,	yysvec+4,	0,	
yycrank+-46,	yysvec+4,	0,	
yycrank+-66,	yysvec+4,	0,	
yycrank+-348,	yysvec+4,	yyvstop+152,
yycrank+-358,	yysvec+4,	yyvstop+154,
yycrank+-163,	yysvec+4,	yyvstop+156,
yycrank+0,	0,		yyvstop+159,
yycrank+-72,	yysvec+4,	yyvstop+163,
yycrank+0,	0,		yyvstop+165,
yycrank+-364,	yysvec+4,	yyvstop+168,
yycrank+-382,	yysvec+4,	yyvstop+171,
yycrank+-84,	yysvec+4,	yyvstop+173,
yycrank+0,	0,		yyvstop+175,
yycrank+-392,	yysvec+4,	yyvstop+178,
yycrank+0,	0,		yyvstop+181,
yycrank+-292,	yysvec+4,	0,	
yycrank+-300,	yysvec+4,	0,	
yycrank+-395,	yysvec+4,	yyvstop+185,
yycrank+-402,	yysvec+4,	yyvstop+187,
yycrank+-404,	yysvec+4,	yyvstop+189,
yycrank+-406,	yysvec+4,	yyvstop+191,
yycrank+-203,	yysvec+4,	yyvstop+193,
yycrank+0,	0,		yyvstop+196,
yycrank+-91,	yysvec+4,	yyvstop+200,
yycrank+0,	0,		yyvstop+202,
yycrank+-408,	yysvec+4,	yyvstop+205,
yycrank+-377,	yysvec+4,	0,	
yycrank+-416,	yysvec+4,	yyvstop+208,
yycrank+-337,	yysvec+4,	yyvstop+211,
yycrank+0,	0,		yyvstop+213,
yycrank+-421,	yysvec+4,	yyvstop+216,
yycrank+0,	0,		yyvstop+218,
yycrank+-423,	yysvec+4,	yyvstop+221,
yycrank+-432,	yysvec+4,	yyvstop+223,
yycrank+-398,	yysvec+4,	yyvstop+226,
yycrank+0,	0,		yyvstop+229,
yycrank+-352,	yysvec+4,	0,	
yycrank+-442,	yysvec+4,	yyvstop+233,
yycrank+-93,	yysvec+4,	yyvstop+236,
yycrank+0,	0,		yyvstop+239,
yycrank+-53,	yysvec+4,	0,	
yycrank+-446,	yysvec+4,	yyvstop+243,
yycrank+-116,	yysvec+4,	0,	
yycrank+-457,	yysvec+4,	yyvstop+245,
yycrank+-411,	yysvec+4,	yyvstop+248,
yycrank+0,	0,		yyvstop+251,
yycrank+-427,	yysvec+4,	0,	
yycrank+-206,	yysvec+4,	yyvstop+255,
yycrank+0,	0,		yyvstop+258,
yycrank+-434,	yysvec+4,	0,	
yycrank+-64,	yysvec+4,	0,	
yycrank+-419,	yysvec+4,	yyvstop+262,
yycrank+-448,	yysvec+4,	0,	
yycrank+-76,	yysvec+4,	0,	
yycrank+-461,	yysvec+4,	yyvstop+264,
yycrank+0,	0,		yyvstop+267,
yycrank+-450,	yysvec+4,	0,	
yycrank+-487,	yysvec+4,	yyvstop+271,
yycrank+-491,	yysvec+4,	yyvstop+274,
yycrank+-495,	yysvec+4,	yyvstop+276,
yycrank+-290,	yysvec+4,	yyvstop+278,
yycrank+0,	0,		yyvstop+281,
yycrank+-313,	yysvec+4,	yyvstop+285,
yycrank+0,	0,		yyvstop+287,
yycrank+-497,	yysvec+4,	yyvstop+290,
yycrank+0,	0,		yyvstop+292,
0,	0,	0};
struct yywork *yytop = yycrank+540;
struct yysvf *yybgin = yysvec+1;
char yymatch[] ={
00  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,011 ,012 ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
011 ,01  ,01  ,01  ,01  ,01  ,01  ,01  ,
01  ,01  ,'*' ,01  ,01  ,'-' ,'*' ,'-' ,
'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,
'*' ,'*' ,01  ,01  ,01  ,'-' ,01  ,01  ,
01  ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,
'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,
'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,
'*' ,'*' ,'*' ,01  ,0134,01  ,01  ,'-' ,
01  ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,
'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,
'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,'*' ,
'*' ,'*' ,'*' ,01  ,01  ,01  ,01  ,01  ,
0};
char yyextra[] ={
0,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,
1,1,1,0,0,0,0,0,
0};
/* Nexix native C scanner driver adapted from the supplied V7 ncform:
 * native-width pointer comparisons, uncompressed miss handling, bytes
 * beyond the character table and tokens longer than yytext. */
#include <stdint.h>
int yylineno =1;
# define YYU(x) x
# define NLSTATE yyprevious=YYNEWLINE
char yytext[YYLMAX];
struct yysvf *yylstate [YYLMAX], **yylsp, **yyolsp;
char yysbuf[YYLMAX];
char *yysptr = yysbuf;
int *yyfnd;
extern struct yysvf *yyestate;
int yyprevious = YYNEWLINE;
yylook(){
	register struct yysvf *yystate, **lsp;
	register struct yywork *yyt;
	struct yysvf *yyz;
	int yych;
	struct yywork *yyr;
# ifdef LEXDEBUG
	int debug;
# endif
	char *yylastch;
	/* start off machines */
# ifdef LEXDEBUG
	debug = 0;
# endif
	if (!yymorfg)
		yylastch = yytext;
	else {
		yymorfg=0;
		yylastch = yytext+yyleng;
		}
	for(;;){
		lsp = yylstate;
		yyestate = yystate = yybgin;
		if (yyprevious==YYNEWLINE) yystate++;
		for (;;){
# ifdef LEXDEBUG
			if(debug)fprintf(yyout,"state %d\n",yystate-yysvec-1);
# endif
			yyt = yystate->yystoff;
			if(yyt == yycrank){		/* may not be any transitions */
				yyz = yystate->yyother;
				if(yyz == 0)break;
				if(yyz->yystoff == yycrank)break;
				}
			if(yylastch >= yytext+YYLMAX-1)break;	/* token fills yytext */
			*yylastch++ = yych = input();
		tryagain:
# ifdef LEXDEBUG
			if(debug){
				fprintf(yyout,"char ");
				allprint(yych);
				putchar('\n');
				}
# endif
			yyr = yyt;
			if ( (uintptr_t)yyt > (uintptr_t)yycrank){
				yyt = yyr + yych;
				if (yyt <= yytop && yyt->verify+yysvec == yystate){
					if(yyt->advance+yysvec == YYLERR)	/* error transitions */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+yysvec;
					goto contin;
					}
				}
# ifdef YYOPTIM
			else if((uintptr_t)yyt < (uintptr_t)yycrank) {		/* r < yycrank */
				yyt = yyr = yycrank+(yycrank-yyt);
# ifdef LEXDEBUG
				if(debug)fprintf(yyout,"compressed state\n");
# endif
				yyt = yyt + yych;
				if(yyt <= yytop && yyt->verify+yysvec == yystate){
					if(yyt->advance+yysvec == YYLERR)	/* error transitions */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+yysvec;
					goto contin;
					}
				/* bytes beyond the character table have no fallback */
				yyt = yych < sizeof(yymatch) ? yyr + YYU(yymatch[yych]) : yytop + 1;
# ifdef LEXDEBUG
				if(debug && yyt <= yytop){
					fprintf(yyout,"try fall back character ");
					allprint(YYU(yymatch[yych]));
					putchar('\n');
					}
# endif
				if(yyt <= yytop && yyt->verify+yysvec == yystate){
					if(yyt->advance+yysvec == YYLERR)	/* error transition */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+yysvec;
					goto contin;
					}
				}
			if ((yystate = yystate->yyother) && (yyt= yystate->yystoff) != yycrank){
# ifdef LEXDEBUG
				if(debug)fprintf(yyout,"fall back to state %d\n",yystate-yysvec-1);
# endif
				goto tryagain;
				}
# endif
			{unput(*--yylastch);break;}
		contin:
# ifdef LEXDEBUG
			if(debug){
				fprintf(yyout,"state %d char ",yystate-yysvec-1);
				allprint(yych);
				putchar('\n');
				}
# endif
			;
			}
# ifdef LEXDEBUG
		if(debug){
			fprintf(yyout,"stopped at %d with ",*(lsp-1)-yysvec-1);
			allprint(yych);
			putchar('\n');
			}
# endif
		while (lsp-- > yylstate){
			*yylastch-- = 0;
			if (*lsp != 0 && (yyfnd= (*lsp)->yystops) && *yyfnd > 0){
				yyolsp = lsp;
				if(yyextra[*yyfnd]){		/* must backup */
					while(yyback((*lsp)->yystops,-*yyfnd) != 1 && lsp > yylstate){
						lsp--;
						unput(*yylastch--);
						}
					}
				yyprevious = YYU(*yylastch);
				yylsp = lsp;
				yyleng = yylastch-yytext+1;
				yytext[yyleng] = 0;
# ifdef LEXDEBUG
				if(debug){
					fprintf(yyout,"\nmatch ");
					sprint(yytext);
					fprintf(yyout," action %d\n",*yyfnd);
					}
# endif
				return(*yyfnd++);
				}
			unput(*yylastch);
			}
		if (yytext[0] == 0  /* && feof(yyin) */)
			{
			yysptr=yysbuf;
			return(0);
			}
		yyprevious = yytext[0] = input();
		if (yyprevious>0)
			output(yyprevious);
		yylastch=yytext;
# ifdef LEXDEBUG
		if(debug)putchar('\n');
# endif
		}
	}
yyback(p, m)
	int *p;
{
if (p==0) return(0);
while (*p)
	{
	if (*p++ == m)
		return(1);
	}
return(0);
}
	/* the following are only used in the lex library */
yyinput(){
	return(input());
	}
yyoutput(c)
  int c; {
	output(c);
	}
yyunput(c)
   int c; {
	unput(c);
	}
