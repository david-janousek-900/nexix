#include <crypt.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#define CHECK(c) do { if(!(c))return __LINE__; } while(0)
static void bits(char out[64],uint64_t value)
{for(int i=0;i<64;++i)out[i]=(char)((value>>(63-i))&1);}
static uint64_t packed(const char in[64])
{uint64_t value=0;for(int i=0;i<64;++i)value=(value<<1)|(unsigned char)in[i];return value;}
int crypt_test(void)
{
    char key[64],block[66];block[0]=0x55;block[65]=0x55;
    bits(key,UINT64_C(0x133457799bbcdff1));bits(block+1,UINT64_C(0x0123456789abcdef));
    setkey(key);encrypt(block+1,0);
    CHECK(packed(block+1)==UINT64_C(0x85e813540f0ab405)&&block[0]==0x55&&block[65]==0x55);
    encrypt(block+1,7);CHECK(packed(block+1)==UINT64_C(0x0123456789abcdef));
    /* DES parity bits do not affect the key schedule. */
    for(int i=7;i<64;i+=8)key[i]^=1;
    setkey(key);encrypt(block+1,0);CHECK(packed(block+1)==UINT64_C(0x85e813540f0ab405));
    const char *passwords[]={"","password","hello","12345678","abcdefgh"};
    const char *salts[]={"..","ab","HX","zz","./"};
    const char *expected[]={"..X8NBuQ4l6uQ","abJnggxhB/yWI","HXWGxfat5w9NQ","zzRtj6pNdfpLE","./GLbXuBxqD4c"};
    char *shared=NULL;
    for(int i=0;i<5;++i){
        char *result=crypt(passwords[i],salts[i]);CHECK(result&&!strcmp(result,expected[i]));
        if(shared)CHECK(shared==result);
        shared=result;
        CHECK(crypt(passwords[i],result)==shared&&!strcmp(shared,expected[i]));
    }
    CHECK(!strcmp(crypt("123456789ignored","zz"),"zzRtj6pNdfpLE"));
    char high[]={ (char)0xf0,(char)0xe1,(char)0xf3,(char)0xf3,(char)0xf7,(char)0xef,(char)0xf2,(char)0xe4,0 };
    CHECK(!strcmp(crypt(high,"ab"),"abJnggxhB/yWI"));
    char one_salt[]={'a',0};char *result=crypt("password",one_salt);
    CHECK(result&&strlen(result)==13&&result[0]=='a'&&result[1]=='a');
    /* setkey must restore unsalted DES after a password-hash operation. */
    bits(key,UINT64_C(0x133457799bbcdff1));bits(block+1,UINT64_C(0x0123456789abcdef));
    setkey(key);encrypt(block+1,0);CHECK(packed(block+1)==UINT64_C(0x85e813540f0ab405));
    char saved[64];memcpy(saved,block+1,64);
    block[1]=2;errno=0;encrypt(block+1,0);CHECK(errno==EINVAL&&block[1]==2);block[1]=saved[0];
    key[0]=2;errno=0;setkey(key);CHECK(errno==EINVAL);
    encrypt(block+1,1);CHECK(packed(block+1)==UINT64_C(0x0123456789abcdef));
    CHECK(crypt(NULL,"ab")==NULL&&errno==EINVAL);
    CHECK(crypt("password",NULL)==NULL&&errno==EINVAL);
    CHECK(crypt("password","")==NULL&&errno==EINVAL);
    return 0;
}
