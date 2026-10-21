/* Private object-writer consumer; installed driver integration is task 184. */
#include "as.h"
#include "archive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    if(argc!=3)return 2;
    struct as_program p;unsigned char *bytes=NULL;size_t size=0;
    int failed=as_parse(argv[1],&p);
    if(!failed)failed=as_object(&p,&bytes,&size);
    if(!failed) {
        unsigned char *again=NULL;size_t other=0;
        failed=as_object(&p,&again,&other);
        if(!failed&&(other!=size||memcmp(bytes,again,size)))failed=1;
        free(again);
    }
    if(!failed)failed=archive_replace(argv[2],bytes,size);
    if(failed)fprintf(stderr,"as-object: cannot produce %s\n",argv[2]);
    free(bytes);as_free(&p);return failed?1:0;
}
