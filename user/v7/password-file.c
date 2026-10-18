#include "password-file.h"
#include <unistd.h>
#include <sys/stat.h>
int v7_password_publish(const char *candidate,const char *password,const char *backup)
{
    if(chown(candidate,0,0) || chmod(candidate,0644))return -1;
    sync();
    /* O_EXCL on the candidate name supplies the command's update lock. Never
     * overwrite a backup left by a previous interrupted publication. */
    if(link(password,backup))return -1;
    sync();
    if(unlink(password)){
        if(unlink(backup))return -2;
        return -1;
    }
    if(link(candidate,password)){
        if(link(backup,password))return -2;
        sync();
        if(unlink(backup))return -2;
        return -1;
    }
    sync();
    if(unlink(backup))return -2;
    sync();return 0;
}
