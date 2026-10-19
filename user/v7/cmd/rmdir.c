/*
 * Remove directory
 */

#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <stdio.h>

int	Errors = 0;
char	*rindex();
char	*strcat();
char	*strcpy();

main(argc,argv)
int argc;
char **argv;
{

	if(argc < 2) {
		fprintf(stderr, "rmdir: arg count\n");
		exit(1);
	}
	while(--argc)
		rmdir(*++argv);
	exit(Errors!=0);
}

rmdir(d)
char *d;
{
	int	fd; ssize_t count;
	char	*np, name[500];
	struct	stat	st, cst;
	struct	direct	dir;

	if(strlen(d)>sizeof(name)-4){fprintf(stderr,"rmdir: name too long\n");++Errors;return;}
	strcpy(name, d);
	if((np = rindex(name, '/')) == NULL)
		np = name;
    else ++np;
	if(stat(name,&st) < 0) {
		fprintf(stderr, "rmdir: %s non-existent\n", name);
		++Errors;
		return;
	}
	if (stat(".", &cst) < 0) {
		fprintf(stderr, "rmdir: cannot stat \"\"");
		++Errors;
		exit(1);
	}
	if((st.st_mode & S_IFMT) != S_IFDIR) {
		fprintf(stderr, "rmdir: %s not a directory\n", name);
		++Errors;
		return;
	}
	if(st.st_ino==cst.st_ino &&st.st_dev==cst.st_dev) {
		fprintf(stderr, "rmdir: cannot remove current directory\n");
		++Errors;
		return;
	}
	if((fd = open(name,0)) < 0) {
		fprintf(stderr, "rmdir: %s unreadable\n", name);
		++Errors;
		return;
	}
	while((count=read(fd, (char *)&dir, sizeof dir)) == sizeof dir) {
		if(dir.d_ino == 0) continue;
		if(dir.d_name[0]=='.' && (!dir.d_name[1] || (dir.d_name[1]=='.' && !dir.d_name[2])))
			continue;
		fprintf(stderr, "rmdir: %s not empty\n", name);
		++Errors;
		close(fd);
		return;
	}
	close(fd);
    if(count<0){fprintf(stderr,"rmdir: %s unreadable\n",name);++Errors;return;}
	if(!strcmp(np, ".") || !strcmp(np, "..")) {
		fprintf(stderr, "rmdir: cannot remove . or ..\n");
		++Errors;
		return;
	}
	strcat(name, "/.");
	if((access(name, 0)) < 0) {		/* name/. non-existent */
		strcat(name, ".");
		goto unl;
	}
	strcat(name, ".");
	if((access(name, 0)) < 0)		/* name/.. non-existent */
		goto unl2;
	if(access(name, 02)) {
		name[strlen(name)-3] = '\0';
		fprintf(stderr, "rmdir: %s: no permission\n", name);
		++Errors;
		return;
	}
unl:
	unlink(name);	/* unlink name/.. */
unl2:
	name[strlen(name)-1] = '\0';
	unlink(name);	/* unlink name/.  */
	name[strlen(name)-2] = '\0';
	if (unlink(name) < 0) {
		fprintf(stderr, "rmdir: %s not removed\n", name);
		++Errors;
	}
}
