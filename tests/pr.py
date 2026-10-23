"""pr: headers, pagination, columns, -m/-t/-s/-w/-l/+page, limits and errors.

Inputs that fit V7's buffers are compared with the V7 source built for the host,
without its second fclose of each input."""
import os
import random
import subprocess
from v7_command import Command
c=Command('pr')
ref=c.dir/'ref.c'
ref.write_text('#include <stdlib.h>\n#include <string.h>\n#include <unistd.h>\n#include <time.h>\n'+open('reference/v7/usr/src/cmd/pr.c').read().replace('extern char *sprintf();','').replace('\t	fclose(file);\n\t	*rbufp = 0376;','\t	*rbufp = 0376;'))
subprocess.run(['cc','-w','-std=gnu89','-funsigned-char',str(ref),'-o',str(c.dir/'ref')],check=True)
env=dict(os.environ,TZ='GMT0')
c.env['TZ']='GMT0'
def f(name,text):
    path=c.file(name,text);os.utime(path,(0,86400*365));return path
a=f('a',''.join(f'line {i}\n' for i in range(1,131)))
b=f('b','alpha\tbeta\ngamma\n')
def same(args,status=0):
    want=subprocess.run([str(c.dir/'ref'),*map(str,args)],capture_output=True,env=env)
    return c.check(args,want.stdout,status).stdout
out=same([b])
assert out.startswith(b'\n\nJan  1 00:00 1971  %s Page 1\n\n\nalpha\tbeta\ngamma\n'%bytes(b)) and out.count(b'\n')==66,out
same([a]);same(['-t',b]);same(['-h','Title','-l12',b]);same(['-3',a]);same(['-2','-s,',a])
assert same(['+3','-t','-l50',a]).startswith(''.join(f'line {i}\n' for i in range(101,131)).encode())
assert c.run(['-0','-t',b]).stdout==c.run(['-1','-t',b]).stdout
c.check(['-80','-w70',b],'',0,stderr='pr: No room for columns.\n')
c.check([c.dir/'missing',b],same([b]),1,stderr=f"pr: can't open {c.dir/'missing'}\n")
c.check(['-m',c.dir/'missing'],'',1)
r=c.run(['-h','H'*500,b]);assert b'H'*500+b' Page 1' in r.stdout and r.returncode==0,r
big=f('big',''.join(f'{"x"*3000}{i}\n' for i in range(60)))
c.check(['-2','-t',big],'',1,stderr='pr: page too large for columns\n')
r=c.run(['-2','-t','-l3',big]);assert r.returncode==0 and r.stdout.count(b'\n')==30,r
rng=random.Random(3)
for trial in range(150):
    text=''.join(''.join(rng.choice('abc \t') for _ in range(rng.randrange(0,30)))+rng.choice(['\n','\n','\n','\f']) for _ in range(rng.randrange(1,150)))
    p=f('r',text);q=f('q',text[::-1].replace('\n','',1)+'\n')
    for flags in ([],['-t'],['-2'],['-3','-t'],['-2','-s:'],['-w40','-4'],['-l20'],['-m'],['-t','-m'],['+2','-l15'],['-t','-s']):
        args=flags+[p]+([q] if '-m' in flags else [])
        want=subprocess.run([str(c.dir/'ref'),*map(str,args)],capture_output=True,env=env)
        c.check(args,want.stdout,0)
print('PASS: pr ASan/UBSan headers, pagination, columns, merge, separators, limits, errors and 1,650 reference comparisons')
