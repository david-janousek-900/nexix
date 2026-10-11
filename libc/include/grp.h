#ifndef NEXIX_LIBC_GRP_H
#define NEXIX_LIBC_GRP_H
/* V7 record fields; database routines belong to step 45. */
struct group { char *gr_name, *gr_passwd; int gr_gid; char **gr_mem; };
struct group *getgrent(void);
struct group *getgrnam(const char *);
struct group *getgrgid(int);
void setgrent(void);
void endgrent(void);
#endif
