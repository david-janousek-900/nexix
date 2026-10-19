/* Native parser for the retained gram.y productions. No target yacc is needed
 * to bootstrap make. Limits and graph actions follow the V7 grammar. */
#include "defs"

extern FILE *fin;
extern char **linesptr;
char *zznextc;
int yylineno;

static int logical_line(char text[INMAX])
{
    size_t used = 0;
    int c, next;
    if (!fin) {
        const char *line = *linesptr++;
        if (!line) return 0;
        if (strlen(line) >= INMAX) fatal("line too long");
        strcpy(text,line); yylineno++; return 1;
    }
    for (;;) {
        c=getc(fin);
        if (c==EOF) {
            if (ferror(fin)) fatal("description read error");
            text[used]=0; return used != 0;
        }
        if (!c) fatal("NUL in description");
        if (c=='\n') {
            yylineno++;
            if (!used || text[used-1]!='\\') { text[used]=0; return 1; }
            text[used-1]=' ';
            do { next=getc(fin); if (next=='\n') yylineno++; }
            while (next==' ' || next=='\t' || next=='\n');
            if (next==EOF) { text[used]=0; return 1; }
            c=next;
        }
        if (used==INMAX-1) fatal("line too long");
        text[used++]=c;
    }
}

static struct nameblock *word(char **cursor)
{
    char *p=*cursor, *start, end;
    struct nameblock *name;
    while (*p==' ' || *p=='\t') p++;
    if (!*p) { *cursor=p; return NULL; }
    start=p;
    while (*p && !(funny[(unsigned char)*p]&TERMINAL)) p++;
    if (p==start) fatal("bad character in dependency line");
    end=*p; *p=0;
    name=srchname(start); if (!name) name=makename(start);
    *p=end;
    if (end && end!=' ' && end!='\t') fatal("bad character in dependency line");
    *cursor=p; return name;
}

int yyparse(void)
{
    char text[INMAX], expanded[OUTMAX], *p, *colon, *semi, *recipe;
    struct nameblock *targets[NLEFTS], *name;
    struct lineblock *current[NLEFTS], *line, *last;
    struct depblock *deps, *dep, *prev;
    struct shblock *shell=NULL, *lastshell=NULL, *command;
    int count=0, n, sep, i, incommand;
    while (logical_line(text)) {
        if (text[0]=='\t') {
            if (!count) fatal("recipe without a target");
            recipe=text+1; goto add_recipe;
        }
        /* A # is a comment before an inline recipe, literal inside it. */
        incommand=0;
        for (p=text; *p; p++) {
            if (*p==';') incommand=1;
            if (*p=='#' && !incommand) { *p=0; break; }
        }
        p=text; while (*p==' ' || *p=='\t') p++;
        if (!*p) continue;
        count=0; shell=lastshell=NULL;
        if (eqsign(p)) continue;
        semi=strchr(p,';'); recipe=NULL;
        if (semi) { *semi=0; recipe=semi+1; }
        subst(p,expanded);
        colon=strchr(expanded,':');
        if (!colon) fatal("Must be a separator on rules line");
        *colon++=0; sep=ALLDEPS;
        if (*colon==':') { colon++; sep=SOMEDEPS; }
        p=expanded; n=0;
        while ((name=word(&p))) {
            if (n==NLEFTS) fatal("Too many lefts");
            targets[n++]=name;
        }
        if (!n) fatal("missing target");
        deps=prev=NULL; p=colon;
        while ((name=word(&p))) {
            dep=ALLOC(depblock); dep->depname=name; dep->nxtdepblock=NULL;
            if (prev) prev->nxtdepblock=dep; else deps=dep;
            prev=dep;
        }
        for (i=0; i<n; i++) {
            name=targets[i];
            if (!name->septype) name->septype=sep;
            else if (name->septype!=sep)
                fprintf(stderr,"Inconsistent rules lines for `%s'\n",name->namep);
            line=ALLOC(lineblock); line->depp=deps; line->shp=NULL;
            line->nxtlineblock=NULL; current[i]=line;
            if (!strcmp(name->namep,".SUFFIXES") && !deps) name->linep=NULL;
            else if (!name->linep) name->linep=line;
            else {
                for (last=name->linep; last->nxtlineblock; last=last->nxtlineblock) ;
                if (sep==ALLDEPS && name->namep[0]=='.') last->shp=NULL;
                last->nxtlineblock=line;
            }
        }
        count=n;
        if (!recipe) continue;
add_recipe:
        while (*recipe==' ' || *recipe=='\t') recipe++;
        command=ALLOC(shblock); command->shbp=copys(recipe); command->nxtshblock=NULL;
        if (lastshell) lastshell->nxtshblock=command; else shell=command;
        lastshell=command;
        for (i=0; i<count; i++) current[i]->shp=shell;
    }
    return 0;
}
