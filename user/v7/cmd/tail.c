/* tail command
 *
 *	tail where [file]
 *	where is +_n[type]
 *	- means n lines before end
 *	+ means nth line from beginning
 *	type 'b' means tail n blocks, not lines
 *	type 'c' means tail n characters
 *	Type 'r' means in lines in reverse order from end
 *	 (for -r, default is entire buffer )
*/
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<errno.h>
#include <unistd.h>
#include <stdlib.h>
#define LBIN 4097
static int tail_write(int fd, const void *data, size_t size)
{
    const char *p = data;
    while (size) {
        ssize_t n = write(fd, p, size);
        if (n <= 0) _exit(1);
        p += n; size -= n;
    }
    return 0;
}
struct stat statb;
char bin[LBIN];
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

main(argc,argv)
char **argv;
{
	long n,di;
	register i,j,k;
	char *p;
	int partial,piped,bylines,bkwds,fromend,lastnl;
	int filearg;
	char *arg;
	filearg = 1;
	arg = argc > 1 ? argv[1] : "-10l";
	if (*arg == '-' || *arg == '+') {
		if (argc > 1) filearg = 2;
	} else arg = "-10l";
	if (argc > filearg + 1) goto errcom;
	fromend = *arg=='-';
	arg++;
	n = 0;
	while(digit(*arg)) {
		if (n > (LONG_MAX - (*arg-'0'))/10) goto errcom;
		n = n*10 + *arg++ - '0';
	}
	if(!fromend&&n>0)
		n--;
	if(argc>filearg) {
		close(0);
		if(open(argv[filearg],0)!=0) {
			tail_write(2,"tail: can't open ",17);
			tail_write(2,argv[filearg],strlen(argv[filearg]));
			tail_write(2,"\n",1);
			exit(1);
		}
	}
	errno = 0;
	piped = lseek(0, 0L, 1) < 0;
	p = bin;
	if (arg[0] && arg[1]) goto errcom;
	bylines = 0; bkwds = 0;
	switch(*arg) {
	case 'b':
		if (n > LONG_MAX/512) goto errcom;
		n *= 512;
		break;
	case 'c':
		break;
	case 'r':
		if(n==0) n = LBIN;
		bkwds = 1; fromend = 1; bylines = 1;
		break;
	case '\0':
	case 'l':
		bylines = 1;
		break;
	default:
		goto errcom;
	}
	if(fromend)
		goto keep;

			/*seek from beginning */

	if(bylines) {
		j = 0;
		while(n-->0) {
			do {
				if(j--<=0) {
					p = bin;
					j = read(0,p,512);
					if(j--<=0) { if(j < -1) goto ioerror; exit(0); }
				}
			} while(*p++ != '\n');
		}
		tail_write(1,p,j);
	} else  if(n>0) {
		if(!piped)
			fstat(0,&statb);
		if(piped||(statb.st_mode&S_IFMT)==S_IFCHR)
			while(n>0) {
				i = n>512?512:n;
				i = read(0,bin,i);
				if(i<=0) { if(i<0) goto ioerror; exit(0); }
				n -= i;
			}
		else
			if (lseek(0,n,0)<0) goto ioerror;
	}
copy:
	while((i=read(0,bin,512))>0)
		tail_write(1,bin,i);
	if (i < 0) goto ioerror;
	exit(0);

			/*seek from end*/

keep:
	if(n<=0) exit(0);
	if(!piped) {
		fstat(0,&statb);
		di = !bylines? n: LBIN-1;
		if(statb.st_size > di)
			if (lseek(0,-di,2)<0) goto ioerror;
		if(!bylines)
			goto copy;
	}
	partial = 1;
	for(;;) {
		i = 0;
		do {
			j = read(0,&bin[i],LBIN-i);
			if(j<=0) {
				if(j<0) goto ioerror;
				goto brka;
			}
			i += j;
		} while(i<LBIN);
		partial = 0;
	}
brka:
	if (partial && i == 0) exit(0);
	if(!bylines) {
		k =
		    n<=i ? i-n:
		    partial ? 0:
		    n>=LBIN ? i+1:
		    i-n+LBIN;
		k--;
	} else {
		if(bkwds && bin[i==0?LBIN-1:i-1]!='\n'){	/* force trailing newline */
			bin[i]='\n';
			if(++i>=LBIN) {i = 0; partial = 0;}
		}
		k = i;
		j = 0;
		do {
			lastnl = k;
			do {
				if(--k<0) {
					if(partial) {
						if(bkwds) tail_write(1,bin,lastnl+1);
						goto brkb;
					}
					k = LBIN -1;
				}
			} while(bin[k]!='\n'&&k!=i);
			if(bkwds && j>0){
				if(k<lastnl) tail_write(1,&bin[k+1],lastnl-k);
				else {
					tail_write(1,&bin[k+1],LBIN-k-1);
					tail_write(1,bin,lastnl+1);
				}
			}
		} while(j++<n&&k!=i);
brkb:
		if(bkwds) exit(0);
		if(k==i) do {
			if(++k>=LBIN)
				k = 0;
		} while(bin[k]!='\n'&&k!=i);
	}
	if(k<i)
		tail_write(1,&bin[k+1],i-k-1);
	else {
		tail_write(1,&bin[k+1],LBIN-k-1);
		tail_write(1,bin,i);
	}
	exit(0);
ioerror:
	tail_write(2,"tail: I/O error\n",16);
	exit(1);
errcom:
	tail_write(2,"usage: tail [+|-number[lbcr]] [file]\n",sizeof("usage: tail [+|-number[lbcr]] [file]\n")-1);
	exit(1);
}

digit(c)
{
	return(c>='0'&&c<='9');
}
