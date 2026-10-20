/* Drive real SIGHUP preservation/recovery as ordinary users. */
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/dir.h>
static int contains(const char *s,const char *needle)
{size_t n=strlen(needle);for(;*s;s++)if(!strncmp(s,needle,n))return 1;return 0;}
int main(int argc,char **argv)
{
 int listing=argc==2 && !strcmp(argv[1],"list");
 int saving=argc==2 && !strcmp(argv[1],"save"),denied=argc==2 && !strcmp(argv[1],"deny");
 int input[2],output[2];char buffer[2048]={0};
 if(argc!=2 || pipe(input) || pipe(output))return 1;
 pid_t pid=fork();if(pid<0)return 2;
 if(!pid){
  close(input[1]);close(output[0]);dup2(input[0],0);dup2(output[1],1);dup2(output[1],2);close(input[0]);close(output[1]);
  if(setgid(denied?101:100) || setuid(denied?101:100))_exit(90);
  char *listargs[]={"vi","-r",0};
  char *saveargs[]={"ex","-","/tmp/lost",0};char *recoverargs[]={"ex","-","-r","/tmp/lost",0};
  char *env[]={"HOME=/home/alice","PATH=/bin","TERM=dumb",0};
  execve("/bin/ex",listing?listargs:saving?saveargs:recoverargs,env);_exit(91);
 }
 close(input[0]);close(output[1]);
 const char *commands=listing?"":saving?"a\nunsaved recovery text\n.\n!echo RECOVERY_READY\n":denied?"q!\n":"w /tmp/recovered\nq!\n";
 if(write(input[1],commands,strlen(commands))!=(int)strlen(commands))return 3;
 if(saving){
  unsigned used=0;
  while(!contains(buffer,"RECOVERY_READY")){
   int n=read(output[0],buffer+used,sizeof(buffer)-1-used);if(n<=0)return 4;used+=(unsigned)n;buffer[used]=0;if(used==sizeof(buffer)-1)return 5;
  }
  if(kill(pid,SIGHUP))return 6;
 }
 close(input[1]);int n;unsigned total=0;
 while((n=read(output[0],buffer+total,sizeof(buffer)-1-total))>0){total+=(unsigned)n;if(total==sizeof(buffer)-1)break;}
 buffer[total]=0;close(output[0]);
 int status;if(wait(&status)!=pid || !WIFEXITED(status) || (denied?!WEXITSTATUS(status):WEXITSTATUS(status))){printf("recovery status %d: %s\n",status,buffer);return 7;}
 if(listing && !contains(buffer,"/tmp/lost"))return 10;
 if(saving){
  int fd=open("/var/preserve",O_RDONLY),found=0;struct direct entry;
  if(fd<0)return 11;
  while(read(fd,&entry,sizeof(entry))==sizeof(entry))if(entry.d_ino && entry.d_name[0]=='E' && entry.d_name[1]=='x'){
   char path[64]="/var/preserve/",name[15];struct stat st;
   memcpy(name,entry.d_name,14);name[14]=0;strcat(path,name);
   if(stat(path,&st) || st.st_uid!=100 || (st.st_mode&0777)!=0600)return 12;
   ++found;
  }
  close(fd);if(found!=1)return 13;
 }
 if(!saving && !denied && !listing){
  int fd=open("/tmp/recovered",O_RDONLY);char text[64]={0};
  if(fd<0 || read(fd,text,sizeof(text))!=22 || strcmp(text,"unsaved recovery text\n"))return 8;
  close(fd);struct stat st;if(stat("/tmp/recovered",&st) || st.st_uid!=100)return 9;
 }
 puts(listing?"VI_RECOVERY_LISTED":saving?"VI_PRESERVED":denied?"VI_RECOVERY_DENIED":"VI_RECOVERED");return 0;
}
