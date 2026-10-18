/* V7 libc/gen/mpx.c and sys/mpxcall.s, native ABI and per-call arguments. */
#include <sys/mx.h>
#include <nexix/syscall.h>
#include <errno.h>
#include <nexix/libc.h>
int mpxcall(int command,const struct mx_args *arguments)
{
    const struct mx_args empty={0};
    if(command==MPXN && !arguments)arguments=&empty;
    long result=nx_syscall6(NX_SYS_MPX,command,(long)arguments,0,0,0,0);
    if(result<0){errno=-result;return -1;}return (int)result;
}
static int call(int command,int a,int b,int c)
{const struct mx_args v={.m_arg={a,b,c}};return mpxcall(command,&v);}
int mpx(const char *name,int mode)
{
    if(!name){errno=EFAULT;return -1;}
    if(!*name)return mpxcall(MPXN,0);
    const struct mx_args v={.m_name=(char *)name,.m_arg={mode,0,0}};return mpxcall(MPX,&v);
}
int chan(int group){return call(CHAN,0,group,0);}
int join(int fd,int group){return call(JOIN,fd,group,0);}
int connect(int fd,int channel,int side){return call(CONNECT,fd,channel,side);}
int disconnect(int fd,int channel,int side){return call(DISCON,fd,channel,side);}
int attach(int index,int group){return call(ATTACH,index,group,0);}
int detach(int index,int group){return call(DETACH,index,group,0);}
int extract(int index,int group,int side){return call(EXTR,index,group,side);}
int debug(int variable,int value){return call(DEBUG,variable,value,0);}
int npgrp(int index,int group,int process_group){return call(NPGRP,index,group,process_group);}
int ckill(int index,int group,int signal){return call(CSIG,index,group,signal);}
