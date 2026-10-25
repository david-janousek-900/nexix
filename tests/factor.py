"""Independent factorization oracle, full V7 input range and native converter cases."""
import random
from v7_command import Command

def oracle(n):
    result='\n';d=2
    while d*d<=n:
        while n%d==0:result+=f'     {d}\n';n//=d
        d+=1
    if n>1:result+=f'     {n}\n'
    return result
if __name__=='__main__':
    c=Command('factor');rng=random.Random(210)
    for n in [1,2,3,4,12,65537,99991,1000003*1000033,2**56-1,*[rng.randrange(1,10**8) for _ in range(100)]]:
        c.check([n],stdout=oracle(n))
    c.check([2**55],stdout='\n'+'     2\n'*55)
    c.check([],data='12\n49\n0\n77\n',stdout=oracle(12)+oracle(49))
    c.check([],data='12\nstop\n77\n',stdout=oracle(12))
    c.check(['1 2'],stdout=oracle(12))
    c.check(['12suffix'],stdout=oracle(12))
    c.check([2**56],stdout='',stderr='Ouch.\n',status=1)
    c.check(['9'*10000],stdout='',stderr='Ouch.\n',status=1)
    c.check([1,2],stdout='',stderr='usage: factor [number]\n',status=1)
    print('PASS: factor exact native arithmetic, 106 independent vectors, 56-bit bounds and retained decimal input (ASan/UBSan)')
