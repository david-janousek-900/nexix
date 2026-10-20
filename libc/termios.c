/* Checked native editor subset, implemented over atomic terminal-state ioctls. */
#include <termios.h>
#include <errno.h>
#include <string.h>
int tcgetattr(int fd,struct termios *t)
{
    struct nx_tty_state s;
    if(!t){errno=EFAULT;return -1;}
    if(ioctl(fd,NX_TIOCGETA,&s)<0)return -1;
    memset(t,0,sizeof(*t));
    unsigned f=s.settings.flags;
    int raw=(f&NX_RAW)!=0,split=(f&NX_TTY_SPLITCR)!=0;
    t->c_iflag=raw?0:IXON;
    if(!raw && (f&(split?NX_TTY_ICRNL:NX_CRMOD)))t->c_iflag|=ICRNL;
    if(f&NX_LCASE)t->c_iflag|=IUCLC;
    t->c_oflag=raw?0:OPOST|(f&NX_TBDELAY);
    if(!raw && (f&(split?NX_TTY_ONLCR:NX_CRMOD)))t->c_oflag|=ONLCR;
    t->c_cflag=CS8|CREAD;
    if((f&(NX_ODDP|NX_EVENP))==NX_ODDP)t->c_cflag|=PARENB|PARODD;
    if((f&(NX_ODDP|NX_EVENP))==NX_EVENP)t->c_cflag|=PARENB;
    t->c_lflag=(f&NX_ECHO?ECHO:0)|(!raw && !(f&NX_TTY_NOSIG)?ISIG:0)|(!(f&(NX_RAW|NX_CBREAK))?ICANON:0);
    t->c_cc[VINTR]=s.chars.intr;t->c_cc[VQUIT]=s.chars.quit;
    t->c_cc[VERASE]=s.settings.erase;t->c_cc[VKILL]=s.settings.kill;
    t->c_cc[VEOF]=s.chars.eof;t->c_cc[VEOL]=s.chars.brk;
    t->c_cc[VSTART]=s.chars.start;t->c_cc[VSTOP]=s.chars.stop;
    t->c_cc[VMIN]=1;t->c_cc[VTIME]=0;
    t->c_ispeed=s.settings.ispeed;t->c_ospeed=s.settings.ospeed;t->__native_flags=f;
    return 0;
}
int tcsetattr(int fd,int action,const struct termios *t)
{
    if(!t){errno=EFAULT;return -1;}
    /* VMIN=1 needs no interbyte timer, including vi's VTIME=1 setting. */
    if(action<0 || action>2 || t->c_iflag&~(ICRNL|IUCLC|IXON) ||
       t->c_oflag&~(OPOST|ONLCR|TABDLY) || t->c_cflag&~(CSIZE|CREAD|PARENB|PARODD) ||
       (t->c_cflag&(CSIZE|CREAD))!=(CS8|CREAD) || t->c_lflag&~(ISIG|ICANON|ECHO) ||
       t->c_ispeed>B115200 || t->c_ospeed>B115200 ||
       (!(t->c_lflag&ICANON) && (t->c_cc[VMIN]!=1 || t->c_cc[VTIME]>1)) ||
       (!(t->c_iflag&IXON) && !(t->__native_flags&NX_RAW))) {errno=EINVAL;return -1;}
    struct nx_tty_state s={0};
    unsigned f=t->__native_flags;
    f&=~(NX_ECHO|NX_LCASE|NX_CRMOD|NX_RAW|NX_CBREAK|NX_TBDELAY|NX_ODDP|NX_EVENP|
         NX_TTY_NOSIG|NX_TTY_ICRNL|NX_TTY_ONLCR);
    if(!!(t->c_iflag&ICRNL)==!!(t->c_oflag&ONLCR)) {
        f&=~NX_TTY_SPLITCR;
        if(t->c_iflag&ICRNL)f|=NX_CRMOD;
    } else f|=NX_TTY_SPLITCR;
    if((f&NX_TTY_SPLITCR) && (t->c_iflag&ICRNL))f|=NX_TTY_ICRNL;
    if(t->c_iflag&IUCLC)f|=NX_LCASE;
    if((f&NX_TTY_SPLITCR) && (t->c_oflag&ONLCR))f|=NX_TTY_ONLCR;
    if(t->c_lflag&ECHO)f|=NX_ECHO;
    if(!(t->c_lflag&ISIG))f|=NX_TTY_NOSIG;
    if(!(t->c_lflag&ICANON))f|=NX_CBREAK;
    if(t->c_cflag&PARENB)f|=t->c_cflag&PARODD?NX_ODDP:NX_EVENP;
    f|=t->c_oflag&TABDLY;
    if(t->__native_flags&NX_RAW && !t->c_iflag && !t->c_oflag && !(t->c_lflag&(ISIG|ICANON))) {
        f&=~(NX_CBREAK|NX_TTY_NOSIG|NX_TTY_SPLITCR|NX_TTY_ICRNL|NX_TTY_ONLCR);
        f|=NX_RAW;
    }
    else if(!(t->c_oflag&OPOST)){errno=EINVAL;return -1;}
    s.settings=(struct nx_sgttyb){t->c_ispeed,t->c_ospeed,t->c_cc[VERASE],t->c_cc[VKILL],f};
    s.chars=(struct nx_tchars){t->c_cc[VINTR],t->c_cc[VQUIT],t->c_cc[VSTART],t->c_cc[VSTOP],t->c_cc[VEOF],t->c_cc[VEOL]};
    s.action=(uint32_t)action;
    return ioctl(fd,NX_TIOCSETA,&s);
}
speed_t cfgetispeed(const struct termios *t){return t->c_ispeed;}
speed_t cfgetospeed(const struct termios *t){return t->c_ospeed;}
int cfsetispeed(struct termios *t,speed_t s){if(s>B115200){errno=EINVAL;return -1;}t->c_ispeed=s;return 0;}
int cfsetospeed(struct termios *t,speed_t s){if(s>B115200){errno=EINVAL;return -1;}t->c_ospeed=s;return 0;}
