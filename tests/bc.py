"""Retained yacc bc + dc integration, all recovered math functions and LP64 bounds."""
from pathlib import Path
import math
from v7_command import Command
ROOT=Path(__file__).resolve().parents[1]
CASES=[
 ('2+3\n','5\n'),
 ('scale=10\n1/3\n','.3333333333\n'),
 ('for(i=0;i<5;i++)i\n','0\n1\n2\n3\n4\n'),
 ('a=2\nwhile(a<10){a=a*2;a}\n','4\n8\n16\n'),
 ('define f(x){\nreturn(x*x)\n}\nf(12)\n','144\n'),
 ('define f(x){\nif(x<=1)return(1)\nreturn(x*f(x-1))\n}\nf(8)\n','40320\n'),
 ('a[9]=77\na[9]\n','77\n'),
 ('999999999999999999^2\n','999999999999999998000000000000000001\n'),
 ('"100% done"\n','100% done'),
 ('1+2','3\n'),
 ('/* comment */\n2%5\n','2\n'),
]
MATH='scale=12\ne(1)\ns(1)\nc(1)\nl(2)\na(1)\nj(0,1)\n'
MATH_VALUES=[math.e,math.sin(1),math.cos(1),math.log(2),math.pi/4,0.7651976865579666]
if __name__=='__main__':
    dc=Command('dc',flags=('-fsigned-char',))
    bc=Command('bc',sources=['build/bc/bc.c'],flags=(f'-DBC_DC="{dc.binary}"',f'-DBC_LIBRARY="{ROOT / "reference/supplemental/v6/lib.b"}"'))
    for source,out in CASES: bc.check([],data=source,stdout=out)
    r=bc.run(['-l'],data=MATH)
    assert r.returncode==0,(r.stdout,r.stderr)
    values=[float(v) for v in r.stdout.splitlines()]
    assert len(values)==6 and all(abs(x-y)<2e-10 for x,y in zip(values,MATH_VALUES)), values
    for source in ['2+*3\n','break\n','"unterminated','/* unterminated','1'*1100+'\n']:
        r=bc.run([],data=source)
        assert r.returncode==1 and b'bc:' in r.stderr,(source[:50],r.returncode,r.stdout,r.stderr)
    f=bc.file('input','7*8\n');bc.check([f],data='9*9\n',stdout='56\n81\n')
    r=bc.run([bc.dir/'missing']);assert r.returncode==1 and b'cannot open input file' in r.stderr,r
    r=bc.run(['-c'],data='2+3\n');dc.check([],data=r.stdout,stdout='5\n')
    bc.check(['-z'],stdout='',stderr='bc: unrecognizable argument\n',status=1)
    print('PASS: bc retained grammar/DC pipeline, all six recovered lib.b functions, recursive/local/array/loop/compile paths and malformed input (ASan/UBSan)')
