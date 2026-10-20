"""Native ex command semantics on disposable full roots."""
from pathlib import Path
import sys
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
files={
 'commands': 'a\nalpha\nbeta\nalpha\n.\n1,$s/alpha/ALPHA/g\n1,$g/beta/d\n1r /tmp/extra\n1,$p\nw /tmp/result\n!echo EX_SHELL\nq!\n',
 'extra': 'extra\n',
 'bad': 'notacommand\nq!\n',
 'nomatch': 'e /tmp/extra\ns/absent/replacement/\nq!\n',
 'writebad': 'a\nkeep\n.\nw /absent/result\nq!\n',
 'filecommands': 'w /tmp/reopened\nq!\n',
}
artifacts={'/tmp/ex':'vi-dev.elf'}
for name,content in files.items():
 artifact='ex-test-'+name;Path('build',artifact).write_text(content);artifacts['/tmp/'+name]=artifact
with private_guest(qemu,iso,ram,'ex',artifacts) as guest:
 guest.expect(r'root@nexix.localdomain# ')
 guest.command('/tmp/ex - < /tmp/commands > /tmp/exlog')
 guest.command('cat /tmp/result',stdout='ALPHA\nextra\nALPHA\n')
 guest.command('cat /tmp/exlog',stdout='ALPHA\nextra\nALPHA\nEX_SHELL\n')
 guest.command('/tmp/ex - /tmp/result < /tmp/filecommands > /tmp/exlog')
 guest.command('cmp /tmp/result /tmp/reopened')
 for case in ['bad','nomatch','writebad']:
  guest.command(f'/tmp/ex - < /tmp/{case} > /tmp/exlog',status=1)
 guest.command('cat /tmp/result',stdout='ALPHA\nextra\nALPHA\n')
print(f'PASS: {ram} native ex append/substitute/global/read/write/reopen/shell/errors/status')
