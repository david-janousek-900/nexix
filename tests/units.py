"""Retained dimensional engine/database checked against independent conversion values."""
from pathlib import Path
import re
from v7_command import Command
DATABASE=Path('reference/v7/usr/lib/units').resolve()
CASES=[('m','cm',100.0),('inch','cm',2.54),('mile','km',1.609344),('hour','sec',3600.0),('kg-m/sec2','newton',1.0),('degree','radian',3.141592653589793/180),('1+3 m','km',1.0),('1|2 m','cm',50.0)]
def values(output):
    return [float(value) for value in re.findall(rb'\t\* ([^\r\n]+)',output)]
if __name__=='__main__':
    c=Command('units')
    for have,want,number in CASES:
        r=c.run([DATABASE],data=have+'\n'+want+'\n')
        assert r.returncode==0,(r.stdout,r.stderr)
        actual=values(r.stdout);assert len(actual)==1 and abs(actual[0]-number)<max(1e-9,abs(number)*1e-6),(have,want,actual,number,r.stdout)
    r=c.run([DATABASE],data='m\nsec\n');assert b'conformability\n' in r.stdout and r.returncode==0
    r=c.run([DATABASE],data='nosuchunit\n');assert b'cannot recognize nosuchunit' in r.stdout
    r=c.run([DATABASE],data='m\ncm');assert values(r.stdout)==[100.0]
    c.check([c.dir/'absent'],stdout='no table\n',status=1)
    for data,message in [('x'*300+'\n','unit name too long'),('1+9999999999999 m\n','exponent too large'),('1|0 m\n','zero denominator'),('1|'*20+'1 m\n','fraction nesting too deep'),('\0','NUL in units input')]:
        r=c.run([DATABASE],data=data);assert r.returncode==1 and ('units: '+message+'\n').encode() in r.stderr,(data[:30],r.stdout,r.stderr)
    letters=lambda n: 'u'+''.join(chr(97+(n//26**i)%26) for i in range(3))
    db=c.file('full',''.join(letters(i)+'\n' for i in range(610)))
    r=c.run([db]);assert r.returncode==1 and b'unit table full' in r.stderr,(r.stdout,r.stderr)
    db=c.file('names',''.join(chr(97+i)*250+'\n' for i in range(25)))
    r=c.run([db]);assert r.returncode==1 and b'unit name storage full' in r.stderr,(r.stdout,r.stderr)
    print('PASS: units retained database/dimensional engine, independent conversions, incompatible units and bounded names/hash/fraction/exponent input (ASan/UBSan)')
