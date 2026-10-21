/* Native public driver and /lib/as2 worker. Neither invokes host tools. */
#include "as.h"
#include "archive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef NX_AS2_PATH
#define NX_AS2_PATH "/lib/as2"
#endif
static int usage(void) {fprintf(stderr,"usage: as [-u] [-o object] [source ...]\n");return 2;}
int main(int argc,char **argv) {
    char **sources=calloc(argc?argc:1,sizeof(*sources));if(!sources)return 1;
    const char *output="a.out";size_t count=0;int options=1,haveoutput=0;
    for(int i=1;i<argc;i++) {
        if(options&&!strcmp(argv[i],"--")){options=0;continue;}
        if(options&&(!strcmp(argv[i],"-u")||!strcmp(argv[i],"--64")||(i==1&&!strcmp(argv[i],"-"))))continue;
        if(options&&!strcmp(argv[i],"-o")) {
            if(haveoutput||i+1>=argc){free(sources);return usage();}
            output=argv[++i];haveoutput=1;continue;
        }
        if(options&&argv[i][0]=='-'&&argv[i][1]){free(sources);return usage();}
        sources[count++]=argv[i];
    }
#ifdef AS_WORKER
    struct as_program p;unsigned char *bytes=NULL;size_t size=0;
    int failed=as_parse_files(count,(const char *const *)sources,&p);
    if(!failed&&as_object(&p,&bytes,&size)) {fprintf(stderr,"as: invalid layout, symbol or relocation\n");failed=1;}
    if(!failed&&archive_replace(output,bytes,size)) {fprintf(stderr,"as: cannot write %s\n",output);failed=1;}
    free(bytes);as_free(&p);free(sources);return failed?1:0;
#else
    (void)output;(void)count;
    free(sources);argv[0]=(char *)NX_AS2_PATH;execv(NX_AS2_PATH,argv);
    fprintf(stderr,"as: cannot execute %s\n",NX_AS2_PATH);return 1;
#endif
}
