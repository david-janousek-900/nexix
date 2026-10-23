"""find: traversal, predicates, operators, -exec/-ok, -cpio, limits and errors.

Host directories are presented in the native V7 16-byte format by a
link-time shim (tests/v7-host/dirshim.c)."""
import os
import struct
import time
from v7_command import Command
c=Command('find',sources=['user/v7/cmd/find.c','tests/v7-host/dirshim.c'],
          flags=['-Itests/v7-host','-Wl,--wrap=open,--wrap=read,--wrap=lseek,--wrap=close,--wrap=stat'])
t=c.dir/'t'
for d in ('a/b','a/c','empty'): (t/d).mkdir(parents=True)
files={'a/x.c':b'int x;\n','a/b/y.c':b'y'*1500,'a/c/z.h':b'','top':b'top\n','.hidden':b'h'}
for name,data in files.items(): (t/name).write_bytes(data)
os.chmod(t/'top',0o751);os.link(t/'top',t/'a/toplink')
old=time.time()-10*86400;os.utime(t/'a/x.c',(old,old))
def run(*args,status=0,stderr=None,data=b''):
    r=c.check(list(args),c.run(list(args),data).stdout,status,data=data,stderr=stderr)
    return sorted(r.stdout.decode().splitlines())
every=['t','t/.hidden','t/a','t/a/b','t/a/b/y.c','t/a/c','t/a/c/z.h','t/a/toplink','t/a/x.c','t/empty','t/top']
assert run('t','-print')==every
assert run('t','-name','*.c','-print')==['t/a/b/y.c','t/a/x.c']
assert run('t','-name','[xz].?','-print')==['t/a/c/z.h','t/a/x.c']
assert run('t','-name','*','-print')==[p for p in every if p!='t/.hidden']
assert run('t','-type','d','-print')==['t','t/a','t/a/b','t/a/c','t/empty']
assert run('t','-type','f','-size','+2','-print')==['t/a/b/y.c']
assert run('t','-type','f','-size','0','-print')==['t/a/c/z.h']
assert run('t','-perm','751','-print')==['t/a/toplink','t/top']
assert run('t','-perm','-001','-type','f','-print')==['t/a/toplink','t/top']
assert run('t','-links','2','-type','f','-print')==['t/a/toplink','t/top']
assert run('t','-inum',str(os.stat(t/'top').st_ino),'-print')==['t/a/toplink','t/top']
assert run('t','-mtime','+5','-print')==['t/a/x.c']
assert run('t','-mtime','-5','-type','f','-name','*.c','-print')==['t/a/b/y.c']
assert run('t','-newer',str(t/'a/x.c'),'-name','*.c','-print')==['t/a/b/y.c']
assert run('t','-user',str(os.getuid()),'-name','top','-print')==['t/top']
assert run('t','-user','99999','-print')==[]
assert run('t','-group',str(os.getgid()),'-name','top','-print')==['t/top']
assert run('t','(','-name','*.c','-o','-name','*.h',')','-print')==['t/a/b/y.c','t/a/c/z.h','t/a/x.c']
assert run('t','!','-type','d','-name','t*','-print')==['t/a/toplink','t/top']
assert run('t','-name','top*','-exec','cat','{}',';')==['top','top']
assert run('t','-name','top','-exec','false',';','-print')==[]
assert run('t','-name','*.c','-ok','echo','{}',';',data=b'y\ny\n')==['t/a/b/y.c','t/a/x.c']
assert run('t','-name','*.c','-ok','echo','{}',';',data=b'n\nn\n')==[]
c.check(['t','-name','*.c','-ok','echo','{}',';'],'',2)
assert run('t/a/','-name','x.c','-print')==['t/a/x.c']
r=c.run([t/'a','-name','x.c','-print']);assert r.stdout==f'{t}/a/x.c\n'.encode(),r
# cpio: V7 binary headers of 13 little-endian shorts, words high-first.
arch=c.dir/'out.cpio'
c.check(['t/a/b','-cpio',arch])
data=arch.read_bytes();names=[];pos=0
while True:
    h=struct.unpack('<13h',data[pos:pos+26]);assert h[0]==0o70707
    size=(h[11]&0xffff)<<16|(h[12]&0xffff);name=data[pos+26:pos+26+h[10]-1].decode()
    pos+=26+h[10];pos+=pos&1;names.append(name)
    if name=='TRAILER!!!':break
    assert data[pos:pos+size]==((t/name.replace('t/','',1)).read_bytes() if size else b'');pos+=size+(size&1)
assert names==['t/a/b','t/a/b/y.c','TRAILER!!!'],names
c.check([],'',1,stderr='Usage: find path-list predicate-list\n')
c.check(['-print'],'',1,stderr='Usage: find path-list predicate-list\n')
c.check(['t','-bogus','x'],'',1,stderr='find: bad option -bogus\n')
c.check(['t','-print',')'],'',1,stderr='find: missing conjunction\n')
c.check(['t','('],'',1)
c.check(['t','-exec','echo'],'',1,stderr='find: incomplete statement\n')
c.check(['t','-user','nosuchuser'],'',1,stderr='find: cannot find -user name\n')
c.check(['t','-perm','9'],'',1,stderr='find: bad option -perm\n')
c.check(['t','-newer','missing'],'',1,stderr='find: cannot access missing\n')
c.check(['nodir/x','-print'],'',2,stderr='find: bad starting directory\n')
c.check(['missing','-print'],'',0,stderr='find: bad status-- missing\n')
c.check(['t','-name','a']+['-o','-name','a']*60,'',1,stderr='find: expression too long\n')
c.check(['t','-name','top','-exec','echo']+['a']*60+[';'],'',1,stderr='find: too many -exec arguments\n')
deep=c.dir/'deep';p=deep
for i in range(80):
    p=p/'d0123456789abc';p.mkdir(parents=True)
r=c.run(['deep','-name','zzz','-print']);assert r.returncode==0 and b'find: pathname too long' in r.stderr,r.stderr[:200]
print('PASS: find ASan/UBSan traversal, names, types, sizes, modes, links, times, users, operators, -exec/-ok, cpio archives, deep trees and errors')
