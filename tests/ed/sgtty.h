#ifndef TEST_ED_SGTTY_H
#define TEST_ED_SGTTY_H
struct sgttyb { unsigned char sg_ispeed,sg_ospeed,sg_erase,sg_kill; unsigned short sg_flags; };
#define ECHO 010
int gtty(int,struct sgttyb *);
int stty(int,const struct sgttyb *);
#endif
