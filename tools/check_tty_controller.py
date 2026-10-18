"""Compare native terminal tables against the actual V7 reference initializers."""
from pathlib import Path
import argparse
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def initializer(source, name):
    return re.search(r'\b'+name+r'\[\]\s*=\s*(\{.*?\})\s*;', source, re.S).group(1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='cc')
    args = parser.parse_args()
    dc = (ROOT/'reference/v7/usr/sys/dev/dc.c').read_text()
    dz = (ROOT/'reference/v7/usr/sys/dev/dz.c').read_text()
    tty = (ROOT/'reference/v7/usr/sys/dev/tty.c').read_text()
    source = '''#include "tty_controller.h"
#include <assert.h>
#include <stdio.h>
extern char partab[];
static const unsigned dc_receive[] = DC_RECEIVE;
static const unsigned dc_transmit[] = DC_TRANSMIT;
static const unsigned dz_rates[] = DZ_RATES;
static const unsigned char mapping[] = MAPPING;
int main(void){
    for(unsigned i=0;i<128;++i){assert(tty_character(i)==(unsigned char)partab[i]);assert(tty_mapping(i)==mapping[i]);}
    for(unsigned i=0;i<16;++i){
        struct nx_sgttyb s={i,i,0,0,NX_ODDP|NX_EVENP};struct tty_parameters p;
        assert((tty_parameters(NX_TTY_DC,&s,&p)==0)==(!!dc_receive[i] && !!dc_transmit[i]));
        assert((tty_parameters(NX_TTY_DZ,&s,&p)==0)==(!!dz_rates[i]));
    }
    puts("PASS: all V7 partab/maptab entries and DC/DZ speed selectors match native controller tables");
}
'''
    for token, value in (('DC_RECEIVE', initializer(dc, 'dcrstab')),
                         ('DC_TRANSMIT', initializer(dc, 'dctstab')),
                         ('DZ_RATES', initializer(dz, 'dz_speeds')),
                         ('MAPPING', initializer(tty, 'maptab'))):
        source = source.replace(token, value)
    with tempfile.TemporaryDirectory(prefix='nexix-tty-source-') as directory:
        folder = Path(directory)
        runner = folder/'compare.c'
        runner.write_text(source)
        binary = folder/'compare'
        subprocess.run(shlex.split(args.cc)+['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                       '-I'+str(ROOT/'kernel/include'), '-I'+str(ROOT/'include'), str(runner),
                       str(ROOT/'kernel/v7/tty_controller.c'),
                       str(ROOT/'reference/v7/usr/sys/dev/partab.c'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
