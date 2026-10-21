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
print(f'PASS: {ram} installed {tool} ELF/archive behavior, long names and malformed inputs')
