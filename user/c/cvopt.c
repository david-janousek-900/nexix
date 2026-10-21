/*
 * cvopt: convert the c1 code table description into assembler data.
 *
 * Native retargeting of the supplied V7 cvopt.c.  The input language is
 * unchanged: shape lines %d1t1,d2t2 (degrees z 1 c r i a e n, type flags
 * w i b f d u s l p, '*' for indirection), %[name:] and %\t[name] string
 * labels, and template lines whose macros are re-coded for cexpr:
 *
 *	A1 -> A	A2 -> B	A  -> O	B1 -> C	B2 -> D	BE -> L	BF -> P
 *	C1 -> E	C2 -> F	F  -> G	H  -> H	R  -> I	R1 -> J	S  -> K
 *	I  -> M	#1 -> #	#2 -> "
 *	F, S, H take the subtree flags * (+1) S (+2) C (+4) 1 (+8) 2 (+16)
 *
 * Native adaptations: the output is ELF64 assembler (struct optab is four
 * bytes, padding and a 64-bit string pointer); template strings go to
 * .rodata and the lists to .data; the list buffer is kept in memory
 * instead of the cvopt.tmp file; macros and '%' are recognised only in
 * template and shape lines, so passthrough text may use native syntax
 * (and templates may name %registers).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	LINESZ	1024

static char	*lists;		/* list section text */
static size_t	nlists, alists;
static int	labno = 1;
static int	listopen;
static int	lineno;

/* the native libc has no strstr */
static char *
find(char *s, const char *t)
{
	size_t n = strlen(t);

	for (; *s; s++)
		if (strncmp(s, t, n) == 0)
			return(s);
	return(NULL);
}

static void
emit(const char *s)
{
	size_t n = strlen(s);

	if (nlists + n + 1 > alists) {
		alists = (nlists + n + 1) * 2 + 4096;
		if ((lists = realloc(lists, alists)) == NULL) {
			fprintf(stderr, "cvopt: out of memory\n");
			exit(1);
		}
	}
	memcpy(lists + nlists, s, n + 1);
	nlists += n;
}

static void
fail(const char *s)
{
	fprintf(stderr, "cvopt: line %d: %s\n", lineno, s);
	exit(1);
}

/* type flags of one shape operand */
static int
flag(char **pp)
{
	register int f;
	char *p = *pp;

	f = 0;
	for (;; p++) {
		switch (*p) {
		case 'w': f = 1; continue;
		case 'i': f = 2; continue;
		case 'b': f = 3; continue;
		case 'f': f = 4; continue;
		case 'd': f = 5; continue;
		case 'u': f = 9; continue;
		case 's': f = 6; continue;
		case 'l': f = 8; continue;
		case 'p': f += 16; continue;
		}
		break;
	}
	*pp = p;
	return(f);
}

/* one degree/type operand of a shape */
static void
operand(char **pp, int *m, int *t)
{
	char *p = *pp;

	while (*p==' ' || *p=='\t')
		p++;
	*t = 0;
	switch (*p++) {
	case 'a': *m = 16; *t = flag(&p); break;
	case 'i': *m = 12; *t = flag(&p); break;
	case 'z': *m = 4; *t = flag(&p); break;
	case 'r': *m = 9; *t = flag(&p); break;
	case '1': *m = 5; *t = flag(&p); break;
	case 'c': *m = 8; break;
	case 'e': *m = 20; *t = flag(&p); break;
	case 'n': *m = 63; *t = flag(&p); break;
	default: fail("bad shape");
	}
	if (*p=='*') {
		*m += 0100;
		p++;
	}
	*pp = p;
}

/* template bytes, translated from the macro language */
static char	tmpl[16384];
static int	ntmpl;

static void
put(int c, int *tabflg)
{
	if (ntmpl >= (int)sizeof(tmpl)-2)
		fail("template too long");
	if (*tabflg) {
		*tabflg = 0;
		c |= 0200;
	}
	tmpl[ntmpl++] = c;
}

static void
convert(char *p)
{
	int tabflg, snlflg, t, c;

	tabflg = snlflg = 0;
	if (*p=='\t')
		p++;
	while ((c = *p++) != '\0' && c != '\n') {
		switch (c) {
		case '\t':
			tabflg++;
			continue;
		case 'A':
			if (*p=='1' || *p=='2') {
				put(*p++ + 'A' - '1', &tabflg);
				continue;
			}
			put('O', &tabflg);
			continue;
		case 'B':
			switch (*p++) {
			case '1': put('C', &tabflg); continue;
			case '2': put('D', &tabflg); continue;
			case 'E': put('L', &tabflg); continue;
			case 'F': put('P', &tabflg); continue;
			}
			fail("bad B macro");
			continue;
		case 'C':
			put(*p++ + 'E' - '1', &tabflg);
			continue;
		case 'R':
			if (*p=='1') {
				p++;
				put('J', &tabflg);
			} else
				put('I', &tabflg);
			continue;
		case 'I':
			put('M', &tabflg);
			continue;
		case '#':
			put(*p++=='1' ? '#' : '"', &tabflg);
			continue;
		case 'F':
			put('G', &tabflg);
			goto subtre;
		case 'H':
			put('H', &tabflg);
			goto subtre;
		case 'S':
			put('K', &tabflg);
		subtre:
			snlflg = 1;
			t = 'A';
			for (;; p++) {
				if (*p=='*') t++;
				else if (*p=='S') t += 2;
				else if (*p=='C') t += 4;
				else if (*p=='1') t += 8;
				else if (*p=='2') t += 16;
				else break;
			}
			put(t, &tabflg);
			continue;
		}
		put(c, &tabflg);
	}
	if (!snlflg)
		put('\n', &tabflg);
}

static void
flushstring(char *labels)
{
	int i;

	printf(".Lcv%d:\n%s", labno, labels);
	tmpl[ntmpl++] = 0;
	for (i = 0; i < ntmpl; i++) {
		printf("%s%d", i%16 ? "," : "\t.byte\t", tmpl[i]&0377);
		if (i%16 == 15 || i == ntmpl-1)
			printf("\n");
	}
	ntmpl = 0;
}

int
main(int argc, char **argv)
{
	char line[LINESZ], entries[8192], labels[1024], target[256], *p, *q;
	int m1, t1, m2, t2, ingroup, intemplate;

	if (argc>1 && freopen(argv[1], "r", stdin) == NULL) {
		fprintf(stderr, "%s?\n", argv[1]);
		return(1);
	}
	if (argc>2 && freopen(argv[2], "w", stdout) == NULL) {
		fprintf(stderr, "%s?\n", argv[2]);
		return(1);
	}
	printf("\t.section\t.rodata\n");
	entries[0] = labels[0] = 0;
	ingroup = intemplate = 0;
	for (;;) {
		if (fgets(line, sizeof(line), stdin) == NULL)
			line[0] = 0;
		else
			lineno++;
		if (strlen(line) == sizeof(line)-1)
			fail("line too long");
		if (intemplate && line[0]=='\t') {
			convert(line);
			continue;
		}
		if (intemplate) {
			flushstring(labels);
			labels[0] = 0;
			labno++;
			intemplate = 0;
		}
		if (line[0]==0)
			break;
		if (line[0]=='%') {
			p = line+1;
			while (*p==' ' || *p=='\t')
				p++;
			if (*p=='[') {
				if ((q = strchr(p, ']')) == NULL)
					fail("bad label");
				*q = 0;
				if (q[-1]==':') {
					/* label of the next template string */
					if (strlen(labels) + strlen(p) + 4 > sizeof(labels))
						fail("too many labels");
					strcat(labels, p+1);
					strcat(labels, "\n");
				} else {
					/* the group shares an existing string */
					if (!ingroup)
						fail("alias without shapes");
					snprintf(target, sizeof(target), "%s", p+1);
					for (q = entries; (q = find(q, "@@")) != NULL; ) {
						memmove(q+strlen(target), q+2, strlen(q+2)+1);
						memcpy(q, target, strlen(target));
					}
					emit(entries);
					entries[0] = 0;
					ingroup = 0;
					labno++;
				}
				continue;
			}
			operand(&p, &m1, &t1);
			if (*p++ != ',')
				fail("bad shape");
			operand(&p, &m2, &t2);
			if (strlen(entries) > sizeof(entries)-128)
				fail("too many shapes");
			sprintf(entries+strlen(entries), "\t.byte\t%d,%d,%d,%d\n\t.zero\t4\n\t.quad\t@@\n",
			    m1, t1, m2, t2);
			ingroup = 1;
			listopen = 1;
			continue;
		}
		if (ingroup) {
			if (line[0]!='\t')
				fail("shapes without template");
			snprintf(target, sizeof(target), ".Lcv%d", labno);
			for (q = entries; (q = find(q, "@@")) != NULL; ) {
				memmove(q+strlen(target), q+2, strlen(q+2)+1);
				memcpy(q, target, strlen(target));
			}
			emit(entries);
			entries[0] = 0;
			ingroup = 0;
			intemplate = 1;
			convert(line);
			continue;
		}
		/* passthrough; a new list label closes the previous list */
		for (p = line; *p && (*p=='_' || *p=='.' || (*p>='a'&&*p<='z') || (*p>='A'&&*p<='Z') || (*p>='0'&&*p<='9')); p++)
			;
		if (*p==':' && p>line && listopen) {
			emit("\t.quad\t0,0\n");
			listopen = 0;
		}
		emit(line);
	}
	if (ingroup)
		fail("shapes without template");
	if (listopen)
		emit("\t.quad\t0,0\n");
	fputs(lists ? lists : "", stdout);
	printf("\t.section\t.note.GNU-stack,\"\",@progbits\n");
	if (fflush(stdout) || ferror(stdout)) {
		fprintf(stderr, "cvopt: write error\n");
		return(1);
	}
	return(0);
}
