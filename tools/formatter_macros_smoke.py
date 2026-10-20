import sys
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'formatter-macros',{'/tmp/nroff':'v7-nroff.elf'}) as g:
 g.expect(r'root@nexix.localdomain# ')
 g.command('cd /usr/man')
 for page,pattern in [('man1/cat.1','concatenate'),('man2/read.2','read from file'),('man3/printf.3s','formatted output'),('man5/passwd.5','password file')]:
  g.command('/tmp/nroff -Tlp -man '+page+' > /tmp/out')
  g.command("grep -c '"+pattern+"' /tmp/out",stdout='1\n')
print('PASS: '+ram+' original manual macros on command/syscall/library/file-format pages')
