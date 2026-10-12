#include <stdio.h>
#include <crypt.h>
#include <errno.h>
int errno;
extern int crypt_test(void);
int main(void)
{
    char unset[64]={0};errno=0;encrypt(unset,0);
    if(errno!=EINVAL)return 1;
    int result=crypt_test();
    if(result){printf("FAIL: crypt fixture line %d\n",result);return result;}
    puts("PASS: DES encryption/decryption vectors, V7 password hashes, parity, shared buffers, truncation and invalid inputs");
    return 0;
}
