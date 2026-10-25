"""Retained V7 dc: independent arithmetic oracles and sanitized native-width paths."""
from decimal import Decimal, localcontext
import random
from v7_command import Command

def cases():
    yield ('2 3+p\n', '5\n')
    yield ('_17 5/p\n', '-3\n')
    yield ('3k 1 8/p\n', '0.125\n')
    yield ('2k 1.25 2.50*p\n', '3.12\n')
    yield ('[123p]sa lax\n', '123\n')
    yield ('1sa 2Sa la p La p la p\n', '2\n2\n1\n')
    yield ('99 0:a 0;ap 77 2047:a 2047;ap\n', '99\n77\n')
    yield ('16i FFp 10o FFp\n', '255\nFF\n')
    yield ('2 20^p\n', '1048576\n')
    yield ('2 3+dp *p\n', '5\n25\n')
    yield ('[ok]P\n', 'ok')
    yield ('12 5%p\n', '2\n')

if __name__ == '__main__':
    c=Command('dc',flags=('-fsigned-char',))
    for source,out in cases():
        c.check([],data=source,stdout=out)
    rng=random.Random(208)
    for _ in range(180):
        a=rng.randrange(-10**55,10**55); b=rng.randrange(1,10**35)
        op=rng.choice(['+','-','*','/','%'])
        quotient=(abs(a)//b)*(-1 if a<0 else 1)
        want={'+':a+b,'-':a-b,'*':a*b,'/':quotient,'%':a-quotient*b}[op]
        spelling=lambda n: str(n).replace('-','_')
        r=c.run([],data=f'{spelling(a)} {b}{op}p\n')
        assert r.returncode==0 and r.stdout.replace(b'\\\n',b'')==f'{want}\n'.encode(),(a,b,op,r.stdout,r.stderr)
    for scale in [0,1,10,30,70]:
        with localcontext() as ctx:
            ctx.prec=100
            want=str(Decimal(2).sqrt().quantize(Decimal(1).scaleb(-scale),rounding='ROUND_DOWN'))
        r=c.run([],data=f'{scale}k 2vp\n')
        assert r.returncode==0 and r.stdout.replace(b'\\\n',b'')==(want+'\n').encode(), (scale,r.stdout,r.stderr)
    f=c.file('commands','123p\n')
    c.check([f],data='456p\n',stdout='123\n456\n')
    c.check([c.dir/'missing'],stdout=f"can't open file {c.dir/'missing'}\n",status=1)
    c.check([],data='[unterminated',stdout='',stderr='dc: unterminated string\n',status=1)
    c.check([],data='9999999999999999999999999999999999999999999999o',stdout='',stderr='dc: output base too large\n',status=1)
    c.check([],data='+' ,stdout='stack empty\n')
    # Recursive macro nesting and long shell text must be bounded, not corrupt memory.
    c.run([],data='[la x 1]sa lax\n')
    c.check([],data='!'+'x'*101+'\n',stdout='',stderr='dc: shell command too long\n',status=1)
    print('PASS: dc retained engine, 180 independent big-integer vectors, decimal roots, bases/registers/arrays, bounded errors (ASan/UBSan)')
