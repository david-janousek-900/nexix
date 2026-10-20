"""Uptime with real sampled CPU load, accounting sessions and unprivileged use."""
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'uptime',{'/tmp/uptime':'v7-uptime.elf','/tmp/upcheck':'uptime-guest.elf'},single_user=False,terminals=2) as g:
 login(g,'root','nexix');login(g,'bob','bob',1,'$ ')
 g.command('/tmp/upcheck',stdout='UPTIME_KERNEL_OK\n')
 g.command('nroff -man /usr/man/man1/uptime.1 > /tmp/upmanual')
 g.command("grep -c 'exponentially damped' /tmp/upmanual",stdout='1\n')
 g.command("/tmp/uptime | grep -c '2 users, load average: '",stdout='1\n')
 g.command("/tmp/uptime | grep -c '2 users, load average: '",1,stdout='1\n',prompt=r'\$ ')
 g.command('/tmp/uptime extra',stdout='usage: uptime\n',status=2)
 g.send('\x04',1);g.expect('login: ',1)
 g.command("/tmp/uptime | grep -c '1 user, load average: '",stdout='1\n')
print('PASS: '+ram+' uptime/load snapshots, real CPU workload, monotonic time, accounting logins and ordinary users')
