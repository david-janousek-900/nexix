/* Native equivalent of V7 __.SYMDEF: GNU/SysV ELF symbol indexes. */
#include "archive.h"
#include <stdio.h>
int main(int argc,char **argv) {
    if(argc<2){fprintf(stderr,"usage: ranlib archive ...\n");return 2;}
    int status=0;
    for(int i=1;i<argc;i++) {
        struct archive a;
        if(archive_load(argv[i],&a)) {fprintf(stderr,"ranlib: invalid archive %s\n",argv[i]);status=1;continue;}
        if(archive_write(argv[i],&a,1)) {fprintf(stderr,"ranlib: cannot index %s\n",argv[i]);status=1;}
        archive_free(&a);
    }
    return status;
}
