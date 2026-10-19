/* Deterministic guest timestamp fixture for dependency acceptance. */
#include <stdlib.h>
#include <utime.h>
int main(int argc, char **argv)
{
    struct utimbuf times;
    if (argc != 3) return 2;
    times.actime=times.modtime=strtol(argv[1],0,10);
    return utime(argv[2],&times) ? 1 : 0;
}
