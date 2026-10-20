/* Explicit native environment: no assumptions about linker ordering or pointer width. */
struct roff_environment {
int block;
int ics;
int ic;
int icf;
int chbits;
int spbits;
int nmbits;
int apts;
int apts1;
int pts;
int pts1;
int font;
int font1;
int sps;
int spacesz;
int lss;
int lss1;
int ls;
int ls1;
int ll;
int ll1;
int lt;
int lt1;
int ad;
int nms;
int ndf;
int fi;
int cc;
int c2;
int ohc;
int tdelim;
int hyf;
int hyoff;
int un1;
int tabc;
int dotc;
int adsp;
int adrem;
int lastl;
int nel;
int admod;
int *wordp;
int spflg;
int *linep;
int *wdend;
int *wdstart;
int wne;
int ne;
int nc;
int nb;
int lnmod;
int nwd;
int nn;
int ni;
int ul;
int cu;
int ce;
int in;
int in1;
int un;
int wch;
int pendt;
int *pendw;
int pendnf;
int spread;
int it;
int itmac;
int lnsize;
int *hyptr[NHYP];
int tabtab[NTAB];
int line[LNSIZE];
int word[WDSIZE];
};
extern struct roff_environment roff_env;
#define EVS ((sizeof(struct roff_environment)+sizeof(int)-1)/sizeof(int))
#define block (roff_env.block)
#define ics (roff_env.ics)
#define ic (roff_env.ic)
#define icf (roff_env.icf)
#define chbits (roff_env.chbits)
#define spbits (roff_env.spbits)
#define nmbits (roff_env.nmbits)
#define apts (roff_env.apts)
#define apts1 (roff_env.apts1)
#define pts (roff_env.pts)
#define pts1 (roff_env.pts1)
#define font (roff_env.font)
#define font1 (roff_env.font1)
#define sps (roff_env.sps)
#define spacesz (roff_env.spacesz)
#define lss (roff_env.lss)
#define lss1 (roff_env.lss1)
#define ls (roff_env.ls)
#define ls1 (roff_env.ls1)
#define ll (roff_env.ll)
#define ll1 (roff_env.ll1)
#define lt (roff_env.lt)
#define lt1 (roff_env.lt1)
#define ad (roff_env.ad)
#define nms (roff_env.nms)
#define ndf (roff_env.ndf)
#define fi (roff_env.fi)
#define cc (roff_env.cc)
#define c2 (roff_env.c2)
#define ohc (roff_env.ohc)
#define tdelim (roff_env.tdelim)
#define hyf (roff_env.hyf)
#define hyoff (roff_env.hyoff)
#define un1 (roff_env.un1)
#define tabc (roff_env.tabc)
#define dotc (roff_env.dotc)
#define adsp (roff_env.adsp)
#define adrem (roff_env.adrem)
#define lastl (roff_env.lastl)
#define nel (roff_env.nel)
#define admod (roff_env.admod)
#define wordp (roff_env.wordp)
#define spflg (roff_env.spflg)
#define linep (roff_env.linep)
#define wdend (roff_env.wdend)
#define wdstart (roff_env.wdstart)
#define wne (roff_env.wne)
#define ne (roff_env.ne)
#define nc (roff_env.nc)
#define nb (roff_env.nb)
#define lnmod (roff_env.lnmod)
#define nwd (roff_env.nwd)
#define nn (roff_env.nn)
#define ni (roff_env.ni)
#define ul (roff_env.ul)
#define cu (roff_env.cu)
#define ce (roff_env.ce)
#define in (roff_env.in)
#define in1 (roff_env.in1)
#define un (roff_env.un)
#define wch (roff_env.wch)
#define pendt (roff_env.pendt)
#define pendw (roff_env.pendw)
#define pendnf (roff_env.pendnf)
#define spread (roff_env.spread)
#define it (roff_env.it)
#define itmac (roff_env.itmac)
#define lnsize (roff_env.lnsize)
#define hyptr (roff_env.hyptr)
#define tabtab (roff_env.tabtab)
#define line (roff_env.line)
#define word (roff_env.word)
