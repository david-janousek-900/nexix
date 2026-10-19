/* Native port of reference/v7/usr/src/cmd/sync.c. */
#include <unistd.h>
int main(void)
{
    sync();
    return 0;
}
