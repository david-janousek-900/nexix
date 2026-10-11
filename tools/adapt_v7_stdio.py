"""Stage checked source adapters for every non-libc private stdio consumer."""
import argparse
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
# Exact replacements fail when pinned reference inputs drift. Remaining program
# adaptations/builds belong to each consumer's roadmap step, not this staging.
ADAPTERS = {
    'cmd/refer/hunt5.c': [('fstat (f->_file, &sb);', 'fstat (fileno(f), &sb);')],
    'cmd/refer/inv1.c': [('\t\tfta = fopen("/dev/null", "w");\n\t\tclose(fta->_file);\n\t\tfta->_file = fw;',
                         '\t\tfta = fdopen(fw, "w");\n\t\t_assert(fta != NULL);')],
    'cmd/refer/what3.c': [('ansf = fopen("/dev/null", "r");\nfmv (fpc[RD], ansf->_file);',
                          'ansf = fdopen(fpc[RD], "r");\n_assert(ansf != NULL);')],
    'games/quiz.c': [('stdout->_flag |= _IONBF;', 'setbuf(stdout, NULL);')],
    'cmd/cpp/cpp.c': [('extern char _sobuf[BUFSIZ];', 'static char nexix_cpp_sobuf[BUFSIZ];'),
                      ('setbuf(fout,_sobuf);', 'setbuf(fout,nexix_cpp_sobuf);')],
}
PRIVATE = re.compile(r'\b(?:_iob|_lastbuf|_sobuf|_sibuf|_filbuf|_flsbuf|_doprnt|_doscan|_IOREAD|_IOWRT|_IONBF|_IOMYBUF|_IOEOF|_IOERR|_IOSTRG|_IORW)\b|(?:->|\.)_(?:cnt|ptr|base|flag|file)\b|\bstruct\s+_iobuf\b')


def adapt_source(name, text):
    for old, new in ADAPTERS[name]:
        if text.count(old) != 1:
            raise ValueError(f'{name}: expected exactly one pinned stdio fragment')
        text = text.replace(old, new)
    if PRIVATE.search(text):
        raise ValueError(f'{name}: remaining private stdio dependency')
    return text


def staged_sources():
    source = ROOT / 'reference/v7/usr/src'
    return {name: adapt_source(name, (source / name).read_text()) for name in ADAPTERS}


def test_fragments(sources):
    """Compile/run the actual adapted call fragments before full consumer ports."""
    hunt = sources['cmd/refer/hunt5.c']
    gdate = hunt[hunt.index('long gdate(f)'):].replace('gdate(f)', 'v7_stdio_gdate(f)', 1)
    def fragment(name, pattern):
        match = re.search(pattern, sources[name])
        if not match:
            raise ValueError('missing adapted test fragment: ' + name)
        return match[0]
    inv = fragment('cmd/refer/inv1.c', r'fta = fdopen\(fw, "w"\);\s*_assert\(fta != NULL\);')
    what = fragment('cmd/refer/what3.c', r'ansf = fdopen\(fpc\[RD\], "r"\);\s*_assert\(ansf != NULL\);')
    quiz = fragment('games/quiz.c', r'setbuf\(stdout, NULL\);')
    cpp = fragment('cmd/cpp/cpp.c', r'static char nexix_cpp_sobuf\[BUFSIZ\];')
    cpp_set = fragment('cmd/cpp/cpp.c', r'setbuf\(fout,nexix_cpp_sobuf\);')
    return ('/* Generated from pinned, adapted V7 source fragments. */\n'
            '#include <stdio.h>\n#include <sys/types.h>\n#include <sys/stat.h>\n#include <assert.h>\n'
            + gdate + '\n'
            + 'FILE *v7_stdio_refer_output(int fw) { FILE *fta; ' + inv + ' return fta; }\n'
            + 'FILE *v7_stdio_refer_input(int fd) { enum {RD=0}; int fpc[2]={fd,-1}; FILE *ansf; ' + what + ' return ansf; }\n'
            + 'void v7_stdio_quiz_output(void) { ' + quiz + ' }\n'
            + 'void v7_stdio_cpp_output(FILE *fout) { ' + cpp + cpp_set + ' }\n')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    sources = staged_sources()
    for name, text in sources.items():
        target = args.output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
    (args.output / 'test-fragments.c').write_text(test_fragments(sources))
    print('PASS: staged five V7 private-stdio consumer adapters and native test fragments')


if __name__ == '__main__': main()
