#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
int libc_scan_test(void)
{
    int integer=99, consumed=0;short small=0;unsigned octal=0,hex=0;
    long wide=0;float real=0;double precise=0;
    char word[16],bytes[4]={0};
    CHECK(sscanf(" -123 177 2a 9223372036854775807 1.25 -3.125e2 end", "%d %o %x %ld %f %lf %3s%n",
        &integer,&octal,&hex,&wide,&real,&precise,word,&consumed)==7);
    CHECK(integer==-123&&octal==127&&hex==42&&wide==INT64_MAX&&real==1.25f&&precise==-312.5);
    CHECK(!strcmp(word,"end")&&consumed==50);
    CHECK(sscanf("32767 -32768", "%hd %hd", &small,&small)==2&&small==INT16_MIN);
    CHECK(sscanf("-9223372036854775808", "%D", &wide)==1&&wide==INT64_MIN);
    CHECK(sscanf("0xff 010 -42", "%i %i %i", &integer,&integer,&integer)==3&&integer==-42);
    CHECK(sscanf("ab cd", "%3c", bytes)==1&&!memcmp(bytes,"ab ",3));
    CHECK(sscanf(" ", "%c", bytes)==1&&bytes[0]==' ');
    CHECK(sscanf("aa-bbb!", "%6[ab-]%c",word,bytes)==2&&!strcmp(word,"aa-bbb")&&bytes[0]=='!');
    CHECK(sscanf("]x!", "%2[]x]%c",word,bytes)==2&&!strcmp(word,"]x")&&bytes[0]=='!');
    CHECK(sscanf("abc:42", "%15[^:]:%d",word,&integer)==2&&!strcmp(word,"abc")&&integer==42);
    CHECK(sscanf("a-z!", "%3[a-z]%c",word,bytes)==2&&!strcmp(word,"a-z")); /* V7 literal set. */
    CHECK(sscanf("12!", "%2d%c",&integer,bytes)==2&&integer==12&&bytes[0]=='!');
    CHECK(sscanf("123", "%1d%2d",&integer,&integer)==2&&integer==23);
    CHECK(sscanf("skip 17", "%*s%d",&integer)==1&&integer==17);
    CHECK(sscanf("%7", "%%%d",&integer)==1&&integer==7);
    CHECK(sscanf("12 x", "%d %d",&integer,&integer)==1&&integer==12);
    CHECK(sscanf("x 12", "%d %d",&integer,&integer)==0&&integer==12);
    CHECK(sscanf("", "%d",&integer)==EOF&&integer==12);
    CHECK(sscanf(" \t\n", "%s",word)==EOF);
    CHECK(sscanf("1.", "%f",&real)==1&&real==1);
    CHECK(sscanf(".5", "%f",&real)==1&&real==.5f);
    CHECK(sscanf("1e+X", "%lf",&precise)==0);
    CHECK(sscanf(".", "%lf",&precise)==0);
    CHECK(sscanf("0xX", "%x",&hex)==0);
    char huge[2048];memset(huge,'9',sizeof(huge)-1);huge[sizeof(huge)-1]=0;
    errno=0;CHECK(sscanf(huge,"%ld",&wide)==1&&wide==INT64_MAX&&errno==ERANGE);
    errno=0;CHECK(sscanf(huge,"%lf",&precise)==1&&errno==ERANGE);
    errno=0;CHECK(sscanf("40000","%hd",&small)==1&&small==INT16_MAX&&errno==ERANGE);
    errno=0;CHECK(sscanf("1e40","%f",&real)==1&&errno==ERANGE);
    char output[128];
    CHECK(snprintf(output,sizeof(output),"%*.*s|%+08d|%#08x|%#.0o",-6,3,"abcdef",42,42,0)>0);
    CHECK(!strcmp(output,"abc   |+0000042|0x00002a|0"));
    CHECK(snprintf(output,sizeof(output),"%8.4d|%.0d|% .2f|%#.3g",12,0,1.5,1.5)>0);
    CHECK(!strcmp(output,"    0012|| 1.50|1.50"));
    CHECK(snprintf(output,sizeof(output),"%.*s/%.*f",-1,"abc",-1,1.)>0&&!strcmp(output,"abc/1.000000"));
    CHECK(snprintf(output,sizeof(output),"%.100f",1.)==-1&&output[0]==0&&errno==EINVAL);
    CHECK(snprintf(output,sizeof(output),"%U %X %O",4294967296UL,0x123456789aUL,0100000000000UL)>0);
    CHECK(!strcmp(output,"4294967296 123456789a 100000000000"));
    /* EOF and high bytes must never index before/after the classification table. */
    CHECK(!isalpha(EOF)&&!isdigit(255)&&!isspace(128)&&!nx_ctype(-2));
    CHECK(isalpha('A')&&islower('z')&&isxdigit('F')&&!isxdigit('g'));
    CHECK(isspace('\v')&&iscntrl(0)&&isascii(127)&&!isascii(128));
    CHECK(toupper('a')=='A'&&tolower('Z')=='z'&&toascii(255)==127);
    strcpy(word,"ab");CHECK(strcat(word,"cd")==word&&!strcmp(word,"abcd"));
    CHECK(strncat(word,"efg",2)==word&&!strcmp(word,"abcdef"));
    CHECK(strncat(word,"xyz",0)==word&&!strcmp(word,"abcdef"));
    CHECK(index(word,'c')==word+2&&rindex("aba",'a')!=NULL);
    char *end;const char *none=" +x";
    CHECK(strtol(" \t-0x2a!",&end,0)==-42&&*end=='!');
    CHECK(strtol("077",&end,0)==63&&!*end);
    CHECK(strtol("0x",&end,0)==0&&*end=='x');
    CHECK(strtol(none,&end,10)==0&&end==none);
    CHECK(strtol("z",&end,36)==35&&!*end);
    CHECK(strtol("-9223372036854775808",&end,10)==INT64_MIN&&!*end);
    errno=0;CHECK(strtol("9223372036854775808!",&end,10)==INT64_MAX&&*end=='!'&&errno==ERANGE);
    CHECK(strtoul("18446744073709551615",&end,10)==UINT64_MAX&&!*end);
    CHECK(strtoul("-1",&end,10)==UINT64_MAX&&!*end);
    errno=0;CHECK(strtoul("18446744073709551616",&end,10)==UINT64_MAX&&errno==ERANGE);
    errno=0;CHECK(strtol("1",&end,1)==0&&*end=='1'&&errno==EINVAL);
    CHECK(atol("1234567890123")==1234567890123L&&atoi("-42")==-42);
    CHECK(abs(-42)==42&&labs(-1234567890123L)==1234567890123L);
    srand(1);CHECK(rand()==16838&&rand()==5758&&rand()==10113);
    return 0;
}
