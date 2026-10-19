/* Native replacement for osh's unavailable /etc/glob helper. V7 directory
 * records, quoted 7-bit patterns, bounded argument lists and sorted matches. */
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <errno.h>
static char *execargs[ARGSIZ+2];
static unsigned execargc;
static int expansion_error;
static int pattern_match(const unsigned char *p,const unsigned char *s)
{
 while(*p){
  unsigned c=*p++;
  if(c=='*'){do{if(pattern_match(p,s))return 1;}while(*s++);return 0;}
  if(!*s)return 0;
  if(c=='?'){++s;continue;}
  if(c=='['){int yes=0,neg=0;unsigned first,last;if(*p=='!'){neg=1;++p;}
   while(*p && *p!=']'){first=*p++&127;last=first;if(*p=='-' && p[1] && p[1]!=']'){++p;last=*p++&127;}if(*s>=first && *s<=last)yes=1;}
   if(*p!=']'||yes==neg)return 0;++p;++s;continue;
  }
  if((c&127)!=*s++)return 0;
 }
 return !*s;
}
static void add_argument(const char *s)
{
 size_t n=strlen(s);char *p;
 if(execargc>=ARGSIZ-1 || n>=LINSIZ){expansion_error=1;return;}
 p=malloc(n+1);if(!p){expansion_error=1;return;}memcpy(p,s,n+1);execargs[execargc++]=p;
}
static int append_component(char *out,const char *prefix,const char *component)
{
 size_t n=strlen(prefix),m=strlen(component);int slash=n&&prefix[n-1]!='/';
 if(n+m+slash>=LINSIZ){expansion_error=1;return 0;}
 memcpy(out,prefix,n);if(slash)out[n++]='/';memcpy(out+n,component,m+1);return 1;
}
static void expand_path(const unsigned char *pattern,const char *prefix,unsigned depth)
{
 unsigned char part[LINSIZ];char plain[LINSIZ],path[LINSIZ];unsigned n=0,j;int wild=0;const unsigned char *rest;struct stat st;
 if(depth>64){expansion_error=1;return;}
 while(*pattern=='/'){prefix=*prefix?prefix:"/";++pattern;}
 while(pattern[n] && pattern[n]!='/'){if(n+1>=LINSIZ){expansion_error=1;return;}part[n]=pattern[n];if(part[n]=='*'||part[n]=='?'||part[n]=='[')wild=1;++n;}
 part[n]=0;rest=pattern+n;while(*rest=='/')++rest;
 for(j=0;j<n;++j)plain[j]=part[j]&127;plain[n]=0;
 if(!wild){if(!append_component(path,prefix,plain))return;if(*rest)expand_path(rest,path,depth+1);else if(!stat(path,&st))add_argument(path);return;}
 int fd=open(*prefix?prefix:".",O_RDONLY);struct direct entry;
 if(fd<0)return;
 if(fstat(fd,&st)<0 || !S_ISDIR(st.st_mode)){close(fd);return;}
 while(read(fd,&entry,sizeof(entry))==sizeof(entry)){
  char name[DIRSIZ+1];if(!entry.d_ino)continue;memcpy(name,entry.d_name,DIRSIZ);name[DIRSIZ]=0;
  if(name[0]=='.' && (part[0]&127)!='.')continue;
  if(!pattern_match(part,(unsigned char *)name))continue;
  if(!append_component(path,prefix,name))break;
  if(*rest)expand_path(rest,path,depth+1);else add_argument(path);
 }
 close(fd);
}
static int compare_arguments(const void *a,const void *b)
{return strcmp(*(char *const *)a,*(char *const *)b);}
static int expand_arguments(char **arguments)
{
 unsigned i;execargc=0;expansion_error=0;
 for(i=0;arguments[i];++i){unsigned start=execargc,j;int wild=0;char plain[LINSIZ];unsigned char *p=(unsigned char *)arguments[i];
  for(j=0;p[j];++j){if(j+1>=LINSIZ)return 1;if(p[j]=='*'||p[j]=='?'||p[j]=='[')wild=1;plain[j]=p[j]&127;}plain[j]=0;
  if(wild){expand_path(p,"",0);if(execargc==start){fputs("No match\n",stderr);return 1;}qsort(execargs+start,execargc-start,sizeof(char *),compare_arguments);}
  else add_argument(plain);
  if(expansion_error){fputs("Too many arguments or characters\n",stderr);return 1;}
 }
 execargs[execargc]=NULL;return 0;
}
