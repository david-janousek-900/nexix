/* Native system identity command; V7 did not supply uname. */
#include <sys/utsname.h>
#include <stdio.h>
#include <string.h>

static int usage(void)
{
    fputs("usage: uname [-asnrvm]\n",stderr);
    return 2;
}

int main(int argc,char **argv)
{
    struct utsname identity;
    unsigned selected=0;
    int i, first=1;
    const char *p, *fields[5];
    for(i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--")) { if(i+1!=argc)return usage(); break; }
        if(argv[i][0]!='-' || !argv[i][1])return usage();
        for(p=argv[i]+1;*p;p++) {
            switch(*p) {
            case 'a': selected=31; break;
            case 's': selected|=1; break;
            case 'n': selected|=2; break;
            case 'r': selected|=4; break;
            case 'v': selected|=8; break;
            case 'm': selected|=16; break;
            default: return usage();
            }
        }
    }
    if(!selected)selected=1;
    if(uname(&identity)<0) { perror("uname");return 1; }
    fields[0]=identity.sysname;fields[1]=identity.nodename;
    fields[2]=identity.release;fields[3]=identity.version;fields[4]=identity.machine;
    for(i=0;i<5;i++)if(selected&(1u<<i)) {
        if(!first)putchar(' ');
        fputs(fields[i],stdout);first=0;
    }
    putchar('\n');
    return fflush(stdout)==EOF || ferror(stdout) ? 1 : 0;
}
