#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char **argv)
{
	char *p;
	size_t n;

	p = malloc(16);
	if (p == NULL)
		exit(1);
	(void)strcpy(p, argc > 1 ? argv[1] : "lint");
	n = strlen(p);
	if (write(1, p, n) != (ssize_t)n)
		return 1;
	(void)fprintf(stdout, "%s %d\n", p, (int)n);
	free(p);
	return 0;
}
