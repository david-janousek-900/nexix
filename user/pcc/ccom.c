/*
 * ccom: the installed portable C compiler pass.
 *
 * ccom [-p] [-l] [-Xflags] [--native-source] [--emit-pass|--read-pass] [input [output]]
 * reads preprocessed C (stdin by default) and writes assembly (stdout).
 * -p calls mcount on function entry and -l annotates expressions with
 * their source lines, as the supplied pcc script requests; -X selects
 * the retained front-end debugging traces.  The native split-pass
 * options replace the supplied convert/fconvert two-pass builds.
 */
#include "backend.h"
#undef printf
#undef puts
#undef putchar
extern int pcc_proflag,pcc_lflag;
int main(int argc,char **argv) {
    unsigned emit=0,read=0;int i,nfiles=0;char *files[2],*passv[16];int passc=1;
    passv[0]=argv[0];
    for(i=1;i<argc;i++) {
        char *a=argv[i];
        if(!strcmp(a,"--emit-pass"))emit=1;
        else if(!strcmp(a,"--read-pass"))read=1;
        else if(!strcmp(a,"--native-source"))pcc_native_source=1;
        else if(!strcmp(a,"-p"))pcc_proflag=1;
        else if(!strcmp(a,"-l"))pcc_lflag=1;
        else if(a[0]=='-' && a[1]=='X' && passc<15)passv[passc++]=a;
        else if(a[0]=='-' && a[1]) {fprintf(stderr,"ccom: bad option %s\n",a);return 2;}
        else if(nfiles<2)files[nfiles++]=a;
        else {fprintf(stderr,"usage: ccom [-p] [-l] [-Xflags] [--native-source] [--emit-pass|--read-pass] [input [output]]\n");return 2;}
    }
    passv[passc]=NULL;
    if(emit && read) {fprintf(stderr,"ccom: --emit-pass and --read-pass conflict\n");return 2;}
    if(nfiles>0 && strcmp(files[0],"-")) {
        if(!freopen(files[0],"r",stdin)) {fprintf(stderr,"ccom: can't open %s\n",files[0]);return 1;}
        snprintf(ftitle,sizeof(ftitle),"%s",files[0]);
    }
    if(nfiles>1 && !freopen(files[1],"w",stdout)) {fprintf(stderr,"ccom: can't create %s\n",files[1]);return 1;}
    if(read) {
        if(pcc_read_pass(stdin)) {fprintf(stderr,"pcc: invalid or failed compiler pass\n");return 1;}
        return 0;
    }
    if(emit && pcc_output_begin())return 1;
    pcc_target_puts(".text");
    int status=mainp1(passc,passv);
    if(!status && pcc_output_end())status=1;
    return status;
}
