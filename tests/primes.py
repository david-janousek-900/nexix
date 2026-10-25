"""Independent sieve and large composite/prime vectors for native V7 primes."""
import os,subprocess
from v7_command import Command
if __name__=='__main__':
    c=Command('primes-check',sources=['tests/primes_check.c'])
    sieve=bytearray(b'\1'*20000);sieve[0:2]=b'\0\0'
    for i in range(2,142):
        if sieve[i]:sieve[i*i::i]=b'\0'*len(sieve[i*i::i])
    c.check([0],stdout=''.join(str(x) for x in sieve))
    for n in [2,3,97,65537,2147483647,72057594037927931]:c.check([n])
    for n in [1,4,9,341,561,2047,1373653,25326001,3215031751,2152302898747,3474749660383,341550071728321,2**56-1,2**56]:c.check([n],status=1)
    # GNU factor is an independent native oracle for the extreme-domain fixture.
    r=subprocess.run(['factor','72057594037927931'],capture_output=True,check=True)
    assert r.stdout==b'72057594037927931: 72057594037927931\n'
    p=Command('primes')
    child=subprocess.Popen([str(p.binary),'90'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=p.env)
    assert [child.stdout.readline() for _ in range(8)]==[f'{x}\n'.encode() for x in [97,101,103,107,109,113,127,131]]
    child.stdout.close();child.wait(timeout=4)
    assert child.returncode in [-13,1],child.returncode
    assert b'Sanitizer' not in child.stderr.read()
    p.check(['72057594037927931'],stdout='72057594037927931\n')
    p.check(['72057594037927932'],stdout='')
    p.check(['0'],stdout='')
    p.check([2**56],stdout='',stderr='Ouch.\n',status=1)
    p.check(['9'*10000],stdout='',stderr='Ouch.\n',status=1)
    print('PASS: primes independent 20,000-number sieve, pseudoprimes, near-2^56 oracle, streaming and bounds (ASan/UBSan)')
