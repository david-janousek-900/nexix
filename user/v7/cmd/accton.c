/* Native adaptation of reference/v7/usr/src/cmd/accton.c. */
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    /* The V7 command uses only its first operand. Check the return value,
     * rather than stale errno, to report syscall failures. */
    if (acct(argc > 1 ? argv[1] : NULL) < 0) {
        perror("accton");
        return 1;
    }
    return 0;
}
