"""Exercise installed native object tools on independently generated files."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram,tool=sys.argv[1:5]
folder=Path('build')/('fixtures-'+tool);artifacts={}
# V7 directory components are limited to fourteen bytes. Payload names remain
# long inside the archive, where the native archive reader must preserve them.
for n in ('object_with_a_very_long_member_name.o','native.a','bsd.a','historical.a','truncated','machine','archive-short','string-offset'):
    artifact='fixtures-'+tool+'/'+n
    path='/tmp/fixture.o' if n.endswith('.o') else '/tmp/'+n
    artifacts[path]=artifact
if tool=='size':artifacts['/tmp/sectionless']='fixtures-size/sectionless'
if tool=='lorder':
    artifacts['/tmp/a.o']='fixtures-lorder/a.o'
    artifacts['/tmp/b.o']='fixtures-lorder/b.o'
with private_guest(qemu,iso,ram,tool,artifacts,single_user=False) as guest:
    login(guest,'root','nexix')
    if tool=='nm':
        guest.command('nm -u /tmp/fixture.o',stdout='unresolved_external\n')
        guest.command("nm -g /tmp/fixture.o | grep -c ' D initialized_global$'",stdout='1\n')
        for archive in ('native.a','bsd.a','historical.a'):
            guest.command('nm -go /tmp/'+archive+" | grep -c 'function_with_a_very_long_symbol_name$'",stdout='1\n')
        for name in ('truncated','machine','archive-short','string-offset'):
            guest.command('nm /tmp/'+name,stdout='nm: cannot read object/archive /tmp/'+name+'\n',status=1)
        guest.command('nm -z',stdout='usage: nm [-goprun] [file ...]\n',status=2)
        guest.command('cp /tmp/fixture.o a.out')
        guest.command('nm -u',stdout='unresolved_external\n')
    if tool=='size':
        line=(folder/'expected.txt').read_text()
        guest.command('size /tmp/fixture.o',stdout=line)
        for archive,member in (('native.a','object_with_a_very_long_member_name.o'),
                               ('bsd.a','object_with_a_very_long_member_name.o'),('historical.a','fixture.o')):
            guest.command('size /tmp/'+archive,stdout=f'/tmp/{archive}({member}): '+line)
        for name in ('truncated','machine','archive-short'):
            guest.command('size /tmp/'+name,stdout='size: cannot read object/archive /tmp/'+name+'\n',status=1)
        guest.command('size -z',stdout='usage: size [file ...]\n',status=2)
        guest.command('cp /tmp/fixture.o a.out')
        guest.command('size',stdout=line)
        guest.command('size /tmp/sectionless',stdout=(folder/'segment.txt').read_text())
    if tool=='lorder':
        guest.command('lorder /tmp/a.o /tmp/b.o',stdout='/tmp/a.o /tmp/a.o\n/tmp/b.o /tmp/b.o\n'+
                      '/tmp/a.o /tmp/b.o\n'*4)
        guest.command('lorder /tmp/a.o',stdout='/tmp/a.o /tmp/a.o\n'*2)
        guest.command('lorder',stdout='usage: lorder file ...\n')
        guest.command('lorder /tmp/absent',stdout='lorder: cannot read object/archive /tmp/absent\n',status=1)
        guest.command("lorder 'with space.o'",stdout='lorder: whitespace in object names is unsupported\n',status=2)
        label='/tmp/native.a(object_with_a_very_long_member_name.o)'
        guest.command('lorder /tmp/native.a',stdout=f'{label} {label}\n')
        for name in ('truncated','machine','archive-short','string-offset'):
            guest.command('lorder /tmp/'+name,stdout='lorder: cannot read object/archive /tmp/'+name+'\n',status=1)
print(f'PASS: {ram} installed {tool} ELF/archive behavior, long names and malformed inputs')
