"""Whole resolver acceptance uses only host-loopback DNS fixtures."""
import sys
from dns_fixture import DNSFixture
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
with DNSFixture(primary=True) as first, DNSFixture() as second, private_guest(qemu,iso,ram,'resolver-acceptance',{'/tmp/res-accept':'resolver-acceptance.elf'},extra_args=('-nic','user,model=e1000')) as guest:
    guest.expect(r'root@[A-Za-z0-9.-]+# ')
    guest.command('/sbin/ifconfig em0 10.0.2.15/24 up; /sbin/route add default 10.0.2.2')
    guest.timeout=45
    guest.command(f'/tmp/res-accept {first.port} {second.port}',stdout='RESOLVER_ACCEPTANCE_OK\n')
    assert sum(host=='silent.test' for host,kind,tcp in first.queries)==2,first.queries
    assert ('truncated.test',1,False) in first.queries and ('truncated.test',1,True) in first.queries
    assert ('failover.test',1,False) in first.queries and ('failover.test',1,False) in second.queries
    assert ('host.missing.search.test',1,False) in first.queries and ('host.search.test',1,False) in first.queries
print(f'PASS: {ram} isolated whole-resolver A/CNAME/PTR, failures, TCP, spoofed replies, retries, failover, search and hosts fallback')
