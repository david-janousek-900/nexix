"""Differential tests against unmodified V7 dsort/dkleave source bytes."""
from pathlib import Path
import argparse
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HEADER = '''#ifndef TEST_BUF_H
#define TEST_BUF_H
#include <stdint.h>
#include <stddef.h>
typedef uint32_t daddr_t;
struct buf {struct buf *av_forw,*av_back;unsigned b_dev,b_flags;uint32_t b_blkno,b_resid;};
#define b_actf av_forw
#define b_actl av_back
#define B_READ 1
#define minor(dev) ((dev)&255)
#endif
'''
RUNNER = r'''
#include "h/buf.h"
#include "disk_order.h"
#include <assert.h>
#include <stdio.h>
int disksort(struct buf *,struct buf *);
daddr_t dkblock(struct buf *);
int dkunit(struct buf *);
static uint32_t seed=0x73951ace;
static uint32_t random_word(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(void)
{
    unsigned round,i;
    for(round=0;round<2048;++round){
        struct buf old[64]={{0}},queue={0};
        struct disk_request native[64]={{0}};struct disk_queue q={0};
        for(i=0;i<64;++i){
            struct buf *a;struct disk_request *b;
            old[i].b_flags=random_word()&1;old[i].b_resid=random_word()%1024;
            native[i].read=!!old[i].b_flags;native[i].cylinder=old[i].b_resid;
            disksort(&queue,&old[i]);disk_sort(&q,&native[i]);
            a=queue.b_actf;b=q.head;
            while(a && b){assert(a-old==b-native);a=a->av_forw;b=b->next;}
            assert(!a && !b && queue.b_actl-old==q.tail-native);
            if((random_word()&3)==0){
                a=queue.b_actf;queue.b_actf=a->av_forw;if(!queue.b_actf)queue.b_actl=NULL;
                assert(disk_pop(&q)==&native[a-old]);
            }
        }
        while(queue.b_actf){struct buf *a=queue.b_actf;queue.b_actf=a->av_forw;assert(disk_pop(&q)==&native[a-old]);}
        assert(!q.head && !q.tail);
    }
    for(i=0;i<256;++i)for(round=0;round<1024;++round){
        struct buf b={0};b.b_dev=i;b.b_blkno=random_word()&0x7fffffff;
        assert(disk_block(i,b.b_blkno)==dkblock(&b));
        assert(disk_unit(i,b.b_blkno)==(unsigned)dkunit(&b));
    }
    puts("PASS: native ordering matches original V7 sources across 131072 insertions and 262144 block/unit mappings");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='cc')
    args = parser.parse_args()
    cc = shlex.split(args.cc)
    with tempfile.TemporaryDirectory(prefix='nexix-v7-order-') as directory:
        folder = Path(directory)
        (folder/'h').mkdir()
        (folder/'dev').mkdir()
        (folder/'h/buf.h').write_text(HEADER)
        (folder/'h/param.h').write_text('#include "buf.h"\n')
        (folder/'h/systm.h').write_text('')
        objects = []
        for name in ('dsort', 'dkleave'):
            source = folder/'dev'/f'{name}.c'
            source.write_bytes((ROOT/f'reference/v7/usr/sys/dev/{name}.c').read_bytes())
            obj = folder/f'{name}.o'
            subprocess.run(cc+['-std=gnu89', '-w', '-c', str(source), '-o', str(obj)], check=True)
            objects.append(str(obj))
        runner = folder/'runner.c'
        runner.write_text(RUNNER)
        binary = folder/'compare'
        subprocess.run(cc+['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                          '-I'+str(ROOT/'kernel/include'), '-I'+str(ROOT/'include'),
                          str(runner), str(ROOT/'kernel/v7/disk_order.c'), *objects, '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
