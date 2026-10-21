/* Private ELF producer; public driver, options and installation are task 187. */
#include "ld.h"
#include "archive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
int main(int argc,char **argv) {
    if(argc<4)return 2;
    struct ld_state s;ld_init(&s);int result=0;unsigned char *bytes=NULL;size_t length=0;
    for(int i=3;i<argc;i++)if(ld_add_file(&s,argv[i])) {result=1;break;}
    if(!result && ld_executable(&s,argv[1],&bytes,&length))result=1;
    if(!result) {
        unsigned char *again=NULL;size_t other=0;
        if(ld_executable(&s,argv[1],&again,&other) || other!=length || memcmp(bytes,again,length))result=1;
        free(again);
    }
    if(!result && (archive_replace(argv[2],bytes,length) || chmod(argv[2],0755)))result=1;
    if(result)fprintf(stderr,"ld-output: %s\n",s.failed?s.error:"cannot write executable");
    free(bytes);ld_free(&s);return result;
}
