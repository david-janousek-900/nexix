#include "tdef.h"
char obuf[OBUFSZ];
char *obufp = obuf;
int r[NN] = {
	PAIR('%',0),
	PAIR('n','l'),
	PAIR('y','r'),
	PAIR('h','p'),
	PAIR('c','t'),
	PAIR('d','n'),
	PAIR('m','o'),
	PAIR('d','y'),
	PAIR('d','w'),
	PAIR('l','n'),
	PAIR('d','l'),
	PAIR('s','t'),
	PAIR('s','b'),
	PAIR('c','.')};
int pto = 10000;
int pfrom = 1;
int print = 1;
char nextf[NS] = "/usr/lib/tmac/tmac.xxxxx";
int nfi = 19;
#ifdef NROFF
char termtab[NS] = "/usr/lib/term/tablp";
int tti = 17;
#endif
#ifndef NROFF
int oldbits = -1;
#endif
int init = 1;
int fc = IMP;
int eschar = '\\';
int pl = 11*INCH;
int po = PO;
int dfact = 1;
int dfactd = 1;
int res = 1;
int smnt = 4;
int ascii = ASCII;
int ptid = PTID;
char ptname[] = "/dev/cat";
int lg = LG;
int pnlist[NPN] = {-1};
int *pnp = pnlist;
int npn = 1;
int npnflg = 1;
int xflg = 1;
int dpn = -1;
int totout = 1;
int ulfont = 1;
int ulbit = 1<<9;
int tabch = TAB;
int ldrch = LEADER;
int xxx;
extern caseds(), caseas(), casesp(), caseft(), caseps(), casevs(),
casenr(), caseif(), casepo(), casetl(), casetm(), casebp(), casech(),
casepn(), tbreak(), caseti(), casene(), casenf(), casece(), casefi(),
casein(), caseli(), casell(), casens(), casemk(), casert(), caseam(),
casede(), casedi(), caseda(), casewh(), casedt(), caseit(), caserm(),
casern(), casead(), casers(), casena(), casepl(), caseta(), casetr(),
caseul(), caselt(), casenx(), caseso(), caseig(), casetc(), casefc(),
caseec(), caseeo(), caselc(), caseev(), caserd(), caseab(), casefl(),
done(), casess(), casefp(), casecs(), casebd(), caselg(), casehc(),
casehy(), casenh(), casenm(), casenn(), casesv(), caseos(), casels(),
casecc(), casec2(), caseem(), caseaf(), casehw(), casemc(), casepm(),
casecu(), casepi(), caserr(), caseuf(), caseie(), caseel(), casepc(),
caseht();
#ifndef NROFF
extern casefz();
#endif
extern casecf();
struct contab {
	int rq;
/*
	union {
 */
		int (*f)();
/*
		unsigned mx;
	}x;
 */
}contab[NM]= {
	PAIR('d','s'),caseds,
	PAIR('a','s'),caseas,
	PAIR('s','p'),casesp,
	PAIR('f','t'),caseft,
	PAIR('p','s'),caseps,
	PAIR('v','s'),casevs,
	PAIR('n','r'),casenr,
	PAIR('i','f'),caseif,
	PAIR('i','e'),caseie,
	PAIR('e','l'),caseel,
	PAIR('p','o'),casepo,
	PAIR('t','l'),casetl,
	PAIR('t','m'),casetm,
	PAIR('b','p'),casebp,
	PAIR('c','h'),casech,
	PAIR('p','n'),casepn,
	PAIR('b','r'),tbreak,
	PAIR('t','i'),caseti,
	PAIR('n','e'),casene,
	PAIR('n','f'),casenf,
	PAIR('c','e'),casece,
	PAIR('f','i'),casefi,
	PAIR('i','n'),casein,
	PAIR('l','i'),caseli,
	PAIR('l','l'),casell,
	PAIR('n','s'),casens,
	PAIR('m','k'),casemk,
	PAIR('r','t'),casert,
	PAIR('a','m'),caseam,
	PAIR('d','e'),casede,
	PAIR('d','i'),casedi,
	PAIR('d','a'),caseda,
	PAIR('w','h'),casewh,
	PAIR('d','t'),casedt,
	PAIR('i','t'),caseit,
	PAIR('r','m'),caserm,
	PAIR('r','r'),caserr,
	PAIR('r','n'),casern,
	PAIR('a','d'),casead,
	PAIR('r','s'),casers,
	PAIR('n','a'),casena,
	PAIR('p','l'),casepl,
	PAIR('t','a'),caseta,
	PAIR('t','r'),casetr,
	PAIR('u','l'),caseul,
	PAIR('c','u'),casecu,
	PAIR('l','t'),caselt,
	PAIR('n','x'),casenx,
	PAIR('s','o'),caseso,
	PAIR('i','g'),caseig,
	PAIR('t','c'),casetc,
	PAIR('f','c'),casefc,
	PAIR('e','c'),caseec,
	PAIR('e','o'),caseeo,
	PAIR('l','c'),caselc,
	PAIR('e','v'),caseev,
	PAIR('r','d'),caserd,
	PAIR('a','b'),caseab,
	PAIR('f','l'),casefl,
	PAIR('e','x'),done,
	PAIR('s','s'),casess,
	PAIR('f','p'),casefp,
	PAIR('c','s'),casecs,
	PAIR('b','d'),casebd,
	PAIR('l','g'),caselg,
	PAIR('h','c'),casehc,
	PAIR('h','y'),casehy,
	PAIR('n','h'),casenh,
	PAIR('n','m'),casenm,
	PAIR('n','n'),casenn,
	PAIR('s','v'),casesv,
	PAIR('o','s'),caseos,
	PAIR('l','s'),casels,
	PAIR('c','c'),casecc,
	PAIR('c','2'),casec2,
	PAIR('e','m'),caseem,
	PAIR('a','f'),caseaf,
	PAIR('h','w'),casehw,
	PAIR('m','c'),casemc,
	PAIR('p','m'),casepm,
#ifdef NROFF
	PAIR('p','i'),casepi,
#endif
	PAIR('u','f'),caseuf,
	PAIR('p','c'),casepc,
	PAIR('h','t'),caseht,
#ifndef NROFF
	PAIR('f','z'),casefz,
#endif
	PAIR('c', 'f'),casecf,
};

/*
troff environment block
*/

#undef block
#undef ics
#undef ic
#undef icf
#undef chbits
#undef spbits
#undef nmbits
#undef apts
#undef apts1
#undef pts
#undef pts1
#undef font
#undef font1
#undef sps
#undef spacesz
#undef lss
#undef lss1
#undef ls
#undef ls1
#undef ll
#undef ll1
#undef lt
#undef lt1
#undef ad
#undef nms
#undef ndf
#undef fi
#undef cc
#undef c2
#undef ohc
#undef tdelim
#undef hyf
#undef hyoff
#undef un1
#undef tabc
#undef dotc
#undef adsp
#undef adrem
#undef lastl
#undef nel
#undef admod
#undef wordp
#undef spflg
#undef linep
#undef wdend
#undef wdstart
#undef wne
#undef ne
#undef nc
#undef nb
#undef lnmod
#undef nwd
#undef nn
#undef ni
#undef ul
#undef cu
#undef ce
#undef in
#undef in1
#undef un
#undef wch
#undef pendt
#undef pendw
#undef pendnf
#undef spread
#undef it
#undef itmac
#undef lnsize
#undef hyptr
#undef tabtab
#undef line
#undef word
struct roff_environment roff_env = {
.block = 0,
.ics = ICS,
.ic = 0,
.icf = 0,
.chbits = 0,
.spbits = 0,
.nmbits = 0,
.apts = PS,
.apts1 = PS,
.pts = PS,
.pts1 = PS,
.font = FT,
.font1 = FT,
.sps = SPS,
.spacesz = SS,
.lss = VS,
.lss1 = VS,
.ls = 1,
.ls1 = 1,
.ll = LL,
.ll1 = LL,
.lt = LL,
.lt1 = LL,
.ad = 1,
.nms = 1,
.ndf = 1,
.fi = 1,
.cc = '.',
.c2 = '\'',
.ohc = OHC,
.tdelim = IMP,
.hyf = 1,
.hyoff = 0,
.un1 = -1,
.tabc = 0,
.dotc = '.',
.adsp = 0,
.adrem = 0,
.lastl = 0,
.nel = 0,
.admod = 0,
.wordp = 0,
.spflg = 0,
.linep = 0,
.wdend = 0,
.wdstart = 0,
.wne = 0,
.ne = 0,
.nc = 0,
.nb = 0,
.lnmod = 0,
.nwd = 0,
.nn = 0,
.ni = 0,
.ul = 0,
.cu = 0,
.ce = 0,
.in = 0,
.in1 = 0,
.un = 0,
.wch = 0,
.pendt = 0,
.pendw = 0,
.pendnf = 0,
.spread = 0,
.it = 0,
.itmac = 0,
.lnsize = LNSIZE,
.hyptr = {0},
.tabtab = {DTAB,DTAB*2,DTAB*3,DTAB*4,DTAB*5,DTAB*6,DTAB*7,DTAB*8,
	DTAB*9,DTAB*10,DTAB*11,DTAB*12,DTAB*13,DTAB*14,DTAB*15,0},
.line = {0},
.word = {0},
};
/*spare 5 words*/
int oline[LNSIZE+1];
