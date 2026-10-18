#ifndef NEXIX_V7_PASSWORD_FILE_H
#define NEXIX_V7_PASSWORD_FILE_H
/* Publish a complete password candidate using V7 links. -1 leaves the original
 * path intact/restored; -2 retains candidate/backup for explicit recovery. */
int v7_password_publish(const char *candidate,const char *password,const char *backup);
#endif
