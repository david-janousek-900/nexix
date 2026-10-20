/* Native declarations; formatter atoi/getline are private parser operations. */
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#define atoi roff_atoi
#define getline roff_getline
char *setbrk(int);
