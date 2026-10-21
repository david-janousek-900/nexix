/* Convert explicit V6/V7 archive containers to the native GNU/SysV format. */
#include "archive.h"
#include <stdio.h>
int main(int argc,char **argv) {
    if(argc<2){fprintf(stderr,"usage: arcv archive ...\n");return 2;}
    int status=0;
    for(int i=1;i<argc;i++) {
        struct archive a;
        if(archive_load(argv[i],&a)) {fprintf(stderr,"arcv: invalid archive %s\n",argv[i]);status=1;continue;}
        if(a.format!=2&&a.format!=3) {fprintf(stderr,"arcv: not a historical archive: %s\n",argv[i]);status=1;}
        else if(archive_write(argv[i],&a,0)) {fprintf(stderr,"arcv: cannot convert %s\n",argv[i]);status=1;}
        archive_free(&a);
    }
    return status;
}
