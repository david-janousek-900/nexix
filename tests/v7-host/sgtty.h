/* Host-test stand-in for the native sgtty interface: no terminal is present. */
struct sgttyb { unsigned char sg_ispeed, sg_ospeed, sg_erase, sg_kill; unsigned sg_flags; };
#define LCASE 04
#define CRMOD 020
#define ALLDELAY 0177400
#define CR1 010000
#define BS1 0100000
#define B150 5
#define B300 7
static int gtty(int fd, struct sgttyb *t) { (void)fd; (void)t; return -1; }
static int stty(int fd, const struct sgttyb *t) { (void)fd; (void)t; return -1; }
