"""V7 ps modes, terminal selection, process lifecycle and unprivileged access."""
import sys,re
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'ps',{'/tmp/ps':'v7-ps.elf'},single_user=False,terminals=2) as g:
 g.expect('login: ');g.send('root\n');g.expect('Password:');g.send('nexix\n');g.expect(r'root@nexix.localdomain# ')
 g.expect('login: ',1);g.send('bob\n',1);g.expect('Password:',1);g.send('bob\n',1);g.expect(r'bob@nexix.localdomain\$ ',1)
 g.command('/tmp/ps ax > /tmp/psout')
 g.command("grep -c 'ps$' /tmp/psout",stdout='1\n')
 g.command('/tmp/ps alx > /tmp/psout')
 g.command("grep -c 'UID.*PPID.*NICE' /tmp/psout",stdout='1\n')
 g.command('/tmp/ps avx > /tmp/psout')
 g.command('/tmp/ps tS1 > /tmp/psout')
 g.command("grep -c 'ttyS1' /tmp/psout",stdout='1\n')
 g.command('/tmp/ps ax 32767 > /tmp/psout',status=1)
 for opt in ['k','z','0','99999999999999999999999999']:
  g.command('/tmp/ps '+opt,stdout='usage: ps [alvx] [tTTY] [PID]\nps: archived kernel-memory mode (-k/namelist) is unavailable on Nexix\n',status=2)
 g.command('/tmp/ps ax > /tmp/bobps',1,prompt=r'bob@nexix.localdomain\$ ')
 g.command("grep -c 'ps$' /tmp/bobps",1,stdout='1\n',prompt=r'bob@nexix.localdomain\$ ')
print('PASS: '+ram+' ps short/long/child-times, terminal/PID filters, malformed requests and ordinary user access')
