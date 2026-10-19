#include "defs"
#include <signal.h>

dosys(comstring,nohalt)
register char *comstring;
int nohalt;
{
register int status;

if(metas(comstring))
	status = doshell(comstring,nohalt);
else	status = doexec(comstring);

return(status);
}



metas(s)   /* Are there are any  Shell meta-characters? */
register char *s;
{
register char c;

while( (funny[c = *s++] & META) == 0 )
	;
return( c );
}

doshell(comstring,nohalt)
char *comstring;
int nohalt;
{
if((childpid = fork()) == 0)
	{
	enbint(SIG_DFL);
	doclose();

	execl(SHELLCOM, "sh", (nohalt ? "-c" : "-ce"), comstring, (char *)0);
	fatal("Couldn't load Shell");
	}

if (childpid < 0) fatal("cannot fork");
return( await() );
}




await()
{

int status;
register int pid;

enbint(intrupt);
while( (pid = wait(&status)) != childpid)
	if(pid == -1)
		fatal("bad wait code");
childpid = 0;
enbint(intrupt);
return(status);
}






doclose()	/* Close open directory files before exec'ing */
{
register struct opendir *od;
for (od = firstod; od; od = od->nxtopendir)
	if (od->dirfc != NULL)
		fclose(od->dirfc);
}





doexec(str)
register char *str;
{
register char *t;
char *argv[200];
register char **p;

while( *str==' ' || *str=='\t' )
	++str;
if( *str == '\0' )
	return(-1);	/* no command */

p = argv;
for(t = str ; *t ; )
	{
	if (p >= argv+199) fatal("too many recipe arguments");
	*p++ = t;
	while(*t!=' ' && *t!='\t' && *t!='\0')
		++t;
	if(*t)
		for( *t++ = '\0' ; *t==' ' || *t=='\t'  ; ++t)
			;
	}

*p = NULL;

if((childpid = fork()) == 0)
	{
	enbint(SIG_DFL);
	doclose();
	execvp(str, argv);
	fatal1("Cannot load %s",str);
	}

if (childpid < 0) fatal("cannot fork");
return( await() );
}

#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>




int touch(force, name)
int force;
char *name;
{
struct stat stbuff;
char junk[1];
int fd;

if( stat(name,&stbuff) < 0)
	if(force)
		goto create;
	else
		{
		fprintf(stderr, "touch: file %s does not exist.\n", name);
		return 1;
		}

if(stbuff.st_size == 0)
	goto create;

if( (fd = open(name, 2)) < 0)
	goto bad;

if( read(fd, junk, 1) < 1)
	{
	close(fd);
	goto bad;
	}
if (lseek(fd, 0L, 0)<0) { close(fd); goto bad; }
if( write(fd, junk, 1) < 1 )
	{
	close(fd);
	goto bad;
	}
if (close(fd)<0) goto bad;
return 0;

bad:
	fprintf(stderr, "Cannot touch %s\n", name);
	return 1;

create:
	if( (fd = creat(name, 0666)) < 0)
		goto bad;
	if (close(fd)<0) goto bad;
	return 0;
}
