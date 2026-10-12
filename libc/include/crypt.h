#ifndef NEXIX_LIBC_CRYPT_H
#define NEXIX_LIBC_CRYPT_H
/* V7 DES interfaces; crypt's result and DES key/salt state are shared. */
char *crypt(const char *password, const char *salt);
void setkey(const char key[64]);
void encrypt(char block[64], int decrypt);
#endif
