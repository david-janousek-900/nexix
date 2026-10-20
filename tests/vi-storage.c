/* Native integration fixture: run the real editor under uid 100, including an
 * existing candidate-name collision, live temp metadata and a full filesystem. */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
static int contains(const char *s,const char *needle)
{size_t n=strlen(needle);for(;*s;s++)if(!strncmp(s,needle,n))return 1;return 0;}
static int equal(const char *a,const char *b)
{
 int x=open(a,O_RDONLY),y=open(b,O_RDONLY);char p[1024],q[1024];int ok=x>=0&&y>=0;
 while(ok){int n=read(x,p,sizeof(p)),m=read(y,q,sizeof(q));if(n!=m || n<0 || (n && memcmp(p,q,n))){ok=0;break;}if(!n)break;}
 if(x>=0)close(x);
 if(y>=0)close(y);
 return ok;
}
static int child_run(int full)
{
 int input[2],output[2],gate[2];pid_t child;char collision[64],candidate[64],captured[1024]={0};
 if(pipe(input)||pipe(output)||pipe(gate))return 10;
 child=fork();if(child<0)return 11;
 if(!child){
  close(gate[1]);char go;if(read(gate[0],&go,1)!=1)_exit(90);close(gate[0]);
  close(input[1]);close(output[0]);dup2(input[0],0);dup2(output[1],1);dup2(output[1],2);close(input[0]);close(output[1]);
  if(setgid(100)||setuid(100))_exit(91);
  char *args[]={"ex","-",0};char *env[]={"HOME=/home/alice","PATH=/bin","TERM=dumb","TERMCAP=dumb:co#80:li#24:",0};
  execve("/tmp/ex",args,env);_exit(92);
 }
 close(input[0]);close(output[1]);close(gate[0]);
 snprintf(collision,sizeof(collision),"/var/tmp/Ex%010d",child);
 if(!full){int fd=creat(collision,0600);if(fd<0)return 12;
 if(write(fd,"sentinel",8)!=8 || close(fd))return 13;}
 write(gate[1],"!",1);close(gate[1]);
 const char *commands=full?"e /tmp/input\nq!\n":"e /tmp/input\n!echo VI_BUFFER_READY\n";
 if(write(input[1],commands,strlen(commands))!=(int)strlen(commands))return 14;
 if(!full){
  unsigned used=0;
  while(!contains(captured,"VI_BUFFER_READY")){
   if(used>=sizeof(captured)-1)return 15;
   int n=read(output[0],captured+used,sizeof(captured)-1-used);if(n<=0)return 16;used+=(unsigned)n;captured[used]=0;
  }
  snprintf(candidate,sizeof(candidate),"/var/tmp/Ex%010d",child+1);
  struct stat st;if(stat(candidate,&st) || (st.st_mode&0777)!=0600 || st.st_uid!=100 || st.st_size<=65536)return 17;
  const char *finish="w /tmp/output\nq!\n";
  if(write(input[1],finish,strlen(finish))!=(int)strlen(finish))return 18;
 }
 close(input[1]);
 unsigned total=0;int n;
 while((n=read(output[0],captured+total,sizeof(captured)-1-total))>0){total+=(unsigned)n;if(total==sizeof(captured)-1)break;}
 captured[total]=0;close(output[0]);
 if(full && !contains(captured,"No space")){printf("missing full-disk error: %s\n",captured);return 22;}
 int status;if(wait(&status)!=child || !WIFEXITED(status) || (!full && WEXITSTATUS(status)) || (full && !WEXITSTATUS(status))){printf("child status %d\n",status);return 19;}
 struct stat st;
 if(!full && (stat(collision,&st) || st.st_size!=8))return 20;
 snprintf(candidate,sizeof(candidate),"/var/tmp/Ex%010d",child+(full?0:1));
 if(stat(candidate,&st)!=-1 || errno!=ENOENT)return 21;
 unlink(collision);
 return 0;
}
int main(void)
{
 int result=child_run(0);if(result){printf("storage failure %d\n",result);return result;}
 if(!equal("/tmp/input","/tmp/output"))return 30;
 int fd=creat("/tmp/fill",0600);char fill[4096];memset(fill,'X',sizeof(fill));
 if(fd<0)return 31;
 unsigned count;for(count=0;count<16384;count++)if(write(fd,fill,sizeof(fill))<0)break;
 int saved=errno;close(fd);if(count==16384 || saved!=ENOSPC)return 32;
 result=child_run(1);unlink("/tmp/fill");
 if(result){printf("full storage failure %d\n",result);return result;}
 if(!equal("/tmp/input","/tmp/output"))return 33;
 puts("VI_STORAGE_OK");return 0;
}
