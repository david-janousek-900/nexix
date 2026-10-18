#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <utmp.h>
#include <time.h>
#include <sys/stat.h>
#define CHECK(c) do{if(!(c)){printf("SESSION_RECORD_ERROR %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    CHECK(argc==2);int phase=atoi(argv[1]);CHECK(phase>=0 && phase<=2);
    struct utmp records[3],history[5];struct stat st;
    int fd=open("/etc/utmp",O_RDONLY);CHECK(fd>=0);
    CHECK(read(fd,records,sizeof(records))==sizeof(records) && read(fd,&st,1)==0 && !close(fd));
    CHECK(!records[0].ut_line[0] && !records[0].ut_name[0] && !records[0].ut_time);
    CHECK(!strncmp(records[1].ut_line,"console",8) && !strncmp(records[2].ut_line,"ttyS1",8));
    CHECK(!strncmp(records[1].ut_name,phase==0?"alice":phase==1?"":"root",8));
    CHECK(!strncmp(records[2].ut_name,phase==2?"":"bob",8));
    time_t now=time(NULL);
    CHECK(records[1].ut_time>=1700000000 && records[1].ut_time<=now && records[2].ut_time>=1700000000 && records[2].ut_time<=now);
    unsigned count=phase==0?2:phase==1?3:5;
    fd=open("/usr/adm/wtmp",O_RDONLY);CHECK(fd>=0);
    CHECK(read(fd,history,count*sizeof(*history))==(ssize_t)(count*sizeof(*history)) && read(fd,&st,1)==0 && !close(fd));
    const char *names[]={"alice","bob","","root",""};const char *lines[]={"console","ttyS1","console","console","ttyS1"};
    for(unsigned i=0;i<count;++i){
        CHECK(!strncmp(history[i].ut_name,names[i],8) && !strncmp(history[i].ut_line,lines[i],8));
        CHECK(history[i].ut_time>=1700000000 && history[i].ut_time<=now);
        if(i)CHECK(history[i].ut_time>=history[i-1].ut_time);
    }
    CHECK(!stat("/etc/utmp",&st) && st.st_mtime>=1700000000);
    CHECK(!stat("/usr/adm/wtmp",&st) && st.st_mtime>=1700000000);
    puts("SESSION_RECORDS_OK");return 0;
}
