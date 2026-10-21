#include <stdio.h>
extern long encoded_integer(long,long);
extern double encoded_float(double,double);
int main(void) {
    if(encoded_integer(3,4)!=19||encoded_integer(-7,5)!=-28)return 1;
    if(encoded_float(1.5,2.0)!=4.5||encoded_float(-2.0,3.0)!=-8.0)return 2;
    puts("ENCODED_ABI_OK");return 0;
}
