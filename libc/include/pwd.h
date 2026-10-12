#ifndef NEXIX_LIBC_PWD_H
#define NEXIX_LIBC_PWD_H
/* V7 record fields with native pointers/int; results share static storage. */
struct passwd {
    char *pw_name, *pw_passwd;
    int pw_uid, pw_gid, pw_quota;
    char *pw_comment, *pw_gecos, *pw_dir, *pw_shell;
};
struct passwd *getpwent(void);
struct passwd *getpwnam(const char *);
struct passwd *getpwuid(int);
void setpwent(void);
void endpwent(void);
int getpw(int, char *);
#endif
