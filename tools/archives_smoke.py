"""Installed archive tools on disposable native filesystems."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram,tool=sys.argv[1:5]
f=Path('build/fixtures-'+tool)
artifacts={}
for name in ('a','b','guest.a','bad.a','v6.a','v6data','historical.a','bsd.a','native.a'):
    if (f/name).exists():
        source='read-'+name if (f/('read-'+name)).exists() else name
        artifacts['/tmp/'+name]='fixtures-'+tool+'/'+source
longname='object_with_a_very_long_member_name.o'
if tool=='arcv':
    artifacts['/tmp/expected.a']='fixtures-arcv/historical.a'
    artifacts['/tmp/expected6.a']='fixtures-arcv/v6.a'
if tool=='ar':artifacts['/tmp/fixture.o']='fixtures-ar/'+longname
with private_guest(qemu,iso,ram,tool,artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    if tool=='ar':
        guest.command('cd /tmp')
        guest.command('ar rcS test.a a b fixture.o')
        guest.command('ar t test.a',stdout='a\nb\nfixture.o\n')
        guest.command('ar s test.a')
        guest.command('ar mb a test.a b')
        guest.command('ar t test.a',stdout='b\na\nfixture.o\n')
        guest.command('ar p test.a b',stdout='B\n')
        guest.command('echo NEW > a')
        guest.command('ar r test.a a')
        guest.command('ar p test.a a',stdout='NEW\n')
        guest.command('ar d test.a b')
        guest.command('ar t test.a',stdout='a\nfixture.o\n')
        guest.command('rm a')
        guest.command('ar x test.a a')
        guest.command('cat a',stdout='NEW\n')
        guest.command('ar q test.a a')
        guest.command('ar t test.a',stdout='a\nfixture.o\na\n')
        guest.command("nm -go test.a | grep -c 'function_with_a_very_long_symbol_name$'",stdout='1\n')
        guest.command('ar t guest.a',stdout='b\na\nname_with_a_long_component\n'+longname+'\nb\n')
        guest.command('ar p v6.a > olddata')
        guest.command('cmp olddata v6data')
        guest.command('cp test.a saved.a')
        guest.command('ar r test.a absent',stdout='ar: cannot read absent\n',status=1)
        guest.command('cmp test.a saved.a')
        guest.command('ar t bad.a',stdout='ar: invalid archive bad.a\n',status=1)
        for archive in ('historical.a','bsd.a','native.a'):
            guest.command('ar p '+archive+' > copy.o')
            guest.command('cmp copy.o fixture.o')
        login(guest,'bob','bob',line=1,prompt='$ ')
        guest.command('cd /tmp',terminal=1)
        guest.command('ar rc bob.a b',terminal=1)
        guest.command('ar p bob.a b',terminal=1,stdout='B\n')
    if tool=='arcv':
        guest.command('arcv /tmp/historical.a /tmp/v6.a')
        guest.command('cmp /tmp/historical.a /tmp/expected.a')
        guest.command('cmp /tmp/v6.a /tmp/expected6.a')
        guest.command("nm -go /tmp/historical.a | grep -c 'function_with_a_very_long_symbol_name$'",stdout='1\n')
        guest.command('cp /tmp/bad.a /tmp/saved.a')
        guest.command('arcv /tmp/bad.a',stdout='arcv: invalid archive /tmp/bad.a\n',status=1)
        guest.command('cmp /tmp/bad.a /tmp/saved.a')
        guest.command('arcv /tmp/historical.a',stdout='arcv: not a historical archive: /tmp/historical.a\n',status=1)
print(f'PASS: {ram} native {tool} archive operations, formats, symbols, failures and unprivileged use')
