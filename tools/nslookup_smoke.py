"""Native BIND grammar generation and installed nslookup against local DNS."""
import sys
from dns_fixture import DNSFixture
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
artifacts={'/tmp/commands.l':'nslookup-commands.l','/tmp/expected.c':'nslookup-commands.c'}
with DNSFixture() as dns, private_guest(qemu,iso,ram,'nslookup',artifacts,extra_args=('-nic','user,model=e1000')) as guest:
    guest.expect(r'root@[A-Za-z0-9.-]+# ')
    guest.command('/sbin/ifconfig em0 10.0.2.15/24 up; /sbin/route add default 10.0.2.2')
    guest.command('mkdir /tmp/nsl; cd /tmp/nsl')
    guest.command('lex /tmp/commands.l')
    guest.command('cmp lex.yy.c /tmp/expected.c')
    guest.command('cd /')
    base=f'/usr/sbin/nslookup -port={dns.port} -timeout=1 -retry=1'
    guest.command(base+' multi.test 10.0.2.2 > /tmp/nsl-out')
    guest.command('grep "^Address: 192" /tmp/nsl-out',stdout='Address: 192.0.2.10\nAddress: 192.0.2.11\n')
    guest.command(base+' -type=PTR 192.0.2.10 10.0.2.2 > /tmp/nsl-out')
    guest.command('grep -c reverse.test /tmp/nsl-out',stdout='1\n')
    guest.command(base+' -type=MX mx.test 10.0.2.2 > /tmp/nsl-out')
    guest.command('grep -c "10 mail.test" /tmp/nsl-out',stdout='1\n')
    guest.command(base+' -vc truncated.test 10.0.2.2 > /tmp/nsl-out')
    guest.command('grep "^Name" /tmp/nsl-out',stdout='Name: truncated.test\n')
    guest.command(base+' missing.test 10.0.2.2 > /tmp/nsl-out',stdout='missing.test: Unknown host\n',status=1)
    guest.send(base+' - 10.0.2.2\n');guest.expect(r'> ')
    def command(line,pattern=None):
        guest.send(line+'\n')
        if pattern:guest.expect(pattern)
        guest.expect(r'> ')
    command('help','Interactive commands')
    command('set type=TXT');command('txt.test',r'"fixture"')
    command('set class=IN');command('set domain=test');command('set search');command('set retry=2');command('set timeout=1')
    command('set vc');command('set debug');command('set all','vc=yes debug=yes search=yes')
    command('set nodebug');command('set novc');command('set type=A')
    command('server 10.0.2.2','Server: 10.0.2.2');command('lserver 10.0.2.2','Server: 10.0.2.2')
    command('ls zone.test',r'ns.test hostmaster.test 1 60 60 60 60[\s\S]*www.zone.test\s+IN\s+A\s+60\s+192.0.2.42')
    guest.send('silent.test\n');guest.expect(r'silent.test\r\n');guest.pump(.2);guest.send(b'\x03');guest.expect(r'> ',timeout=5)
    command('multi.test','Address: 192.0.2.11')
    guest.send(b'\x03');guest.expect(r'> ',timeout=5)
    command('multi.test','Address: 192.0.2.11')
    guest.send('exit\n');guest.expect(r'root@[A-Za-z0-9.-]+# ')
print(f'PASS: {ram} native lex reproduction, nslookup CLI/interactive/options, AXFR and SIGINT')
