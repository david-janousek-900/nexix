#include <stdio.h>
#include <string.h>
int main(int argc,char **argv)
{
    static unsigned char b[32768];FILE *f;size_t n;unsigned i,high=0,control=0;
    if(argc!=3 || !(f=fopen(argv[2],"r")))return 2;
    n=fread(b,1,sizeof b,f);if(ferror(f) || !feof(f)){fclose(f);return 3;}fclose(f);
    if(n<16)return 4;
    if(!strcmp(argv[1],"cat")){
        if(b[0]!=0100 || b[1]!=0357 || b[2]!=0140)return 5;
        if(b[n-1]==0)n--; /* V7 flush terminator. */
        for(i=0;i<n;i++)if(b[i]&0200)high++;
        if(!high || b[n-10]!=0100 || b[n-9]!=0111){printf("bad CAT trailer n=%lu:",(unsigned long)n);for(i=n-12;i<n;i++)printf(" %02x",b[i]);putchar('\n');return 6;}
        for(i=n-8;i<n;i++)if(b[i]!=0101)return 7;
    }else{
        if(b[0]!=037)return 8;
        for(i=0;i<n;i++)if(b[i]==035)control++;
        if(!control || b[n-1]!=037)return 9;
    }
    puts("PROTOCOL_OK");return 0;
}
