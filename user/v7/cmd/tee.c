/*
 * tee-- pipe fitting
 */

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
int *openf;
int n = 1;
int t = 0;
int aflag;
int status;

char in[512];

char out[512];

main(argc,argv)
char **argv;
{
	int register r,w,p;
	struct stat buf;
	while(argc>1&&argv[1][0]=='-') {
		switch(argv[1][1]) {
		case 'a':
			aflag++;
			break;
		case 'i':
		case 0:
			signal(SIGINT, SIG_IGN);
		}
		argv++;
		argc--;
	}
	/* One descriptor per named file rather than V7's fixed 20. */
	if((openf = malloc(sizeof *openf * argc)) == NULL) {
		teeputs("tee: out of memory\n");
		exit(1);
	}
	openf[0] = 1;
	fstat(1,&buf);
	t = (buf.st_mode&S_IFMT)==S_IFCHR;
	if(lseek(1,0L,1)==-1&&errno==ESPIPE)
		t++;
	while(argc-->1) {
		if(aflag) {
			openf[n] = open(argv[1],1);
			if(openf[n] < 0)
				openf[n] = creat(argv[1],0666);
			if(openf[n] >= 0)
				lseek(openf[n],0L,2);
		} else
			openf[n] = creat(argv[1],0666);
		if(openf[n] >= 0 && fstat(openf[n],&buf)>=0) {
			if((buf.st_mode&S_IFMT)==S_IFCHR)
				t++;
			n++;
		} else {
			teeputs("tee: cannot open ");
			teeputs(argv[1]);
			teeputs("\n");
			status = 1;
		}
		argv++;
	}
	r = w = 0;
	for(;;) {
		for(p=0;p<512;) {
			if(r>=w) {
				if(t>0&&p>0) break;
				w = read(0,in,512);
				r = 0;
				if(w<=0) {
					stash(p);
					if(w<0) {
						teeputs("tee: read error\n");
						status = 1;
					}
					exit(status);
				}
			}
			out[p++] = in[r++];
		}
		stash(p);
	}
}

stash(p)
{
	int k;
	int i;
	int d;
	d = t ? 16 : p;
	for(i=0; i<p; i+=d)
		for(k=0;k<n;k++)
			if(openf[k] >= 0 && !writeall(openf[k], out+i, d<p-i?d:p-i)) {
				/* Keep copying to the remaining outputs. */
				openf[k] = -1;
				status = 1;
			}
}

writeall(fd, s, len)
char *s;
{
	int w;
	while(len > 0) {
		if((w = write(fd, s, len)) <= 0)
			return 0;
		s += w;
		len -= w;
	}
	return 1;
}

teeputs(s)
char *s;
{
	while(*s)
		write(2,s++,1);
}
