"""Retained equation grammar and native formatter integration."""
import sys
from pathlib import Path
from private_guest import private_guest
qemu,iso,ram,program=sys.argv[1:5]
Path('build/equation-input').write_text('''.TH EQTEST 1
.SH NAME
Equation fixture
.EQ
x sup 2 + sqrt { a over b }
.EN
.EQ
left ( sum from i=0 to n {x sub i} right )
.EN
.EQ
matrix { lcol { a above b } rcol { c above d } }
.EN
.EQ
define zz /alpha + beta/
zz
.EN
.EQ
fwd 20 x + up 10 y + back 5 z + down 5 t
.EN
''')
Path('build/equation-inline').write_text('''.EQ
delim $$
.EN
inline $x sup 2$ text
''')
artifacts={'/tmp/'+program:'v7-'+program+'.elf','/tmp/equation':'equation-input','/tmp/inline':'equation-inline','/tmp/troff':'v7-troff.elf'}
with private_guest(qemu,iso,ram,program,artifacts) as g:
 g.expect(r'root@nexix.localdomain# ')
 g.command('/tmp/'+program+' /tmp/equation > /tmp/eqout')
 g.command("grep -c '^.EQ' /tmp/eqout",stdout='5\n')
 g.command("grep -c '^.EN' /tmp/eqout",stdout='5\n')
 g.command("grep 'fatal error' /tmp/eqout",status=1)
 formatter='nroff' if program=='neqn' else '/tmp/troff -a'
 g.command(formatter+' -man /tmp/eqout > /tmp/eqrender')
 g.command('test -s /tmp/eqrender')
 g.command('/tmp/'+program+' /tmp/inline > /tmp/eqout')
 g.command("grep -c 'inline' /tmp/eqout",stdout='1\n')
 g.command('/tmp/'+program+' /tmp/absent > /tmp/eqout',stdout='eqn: fatal error: can\'t open file /tmp/absent\nfile /tmp/absent, between lines 0 and 1\n',status=1)
print('PASS: '+ram+' '+program+' powers, roots, fractions, sums, matrices, definitions, inline equations and formatter integration')
