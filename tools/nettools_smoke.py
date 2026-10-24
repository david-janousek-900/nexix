"""ping and nc through the installed commands (task 428)."""
import sys
from private_guest import private_guest
qemu, iso, ram = sys.argv[1:4]
PING_USAGE = 'usage: ping [-n] [-c count] [-i seconds] [-W seconds] [-s size] address\n'
NC_USAGE = 'usage: nc [-u] [-w seconds] address port\n       nc -l [-u] [-w seconds] port\n'
with private_guest(qemu, iso, ram, 'nettools', {'/tmp/as-user': 'as-user.elf'},
                   extra_args=('-nic', 'user,model=e1000')) as g:
    g.expect(r'root@[A-Za-z0-9.-]+# ')
    def c(line, **expect):
        # A non-interactive shell does not print background PIDs.
        g.command("/bin/sh -c '" + line + "'" if ' & ' in line else line, **expect)
    # ping over loopback: replies, statistics, payload size, an ordinary user.
    c('/sbin/ping -n -c 3 127.0.0.1 > /tmp/p')
    c('grep -c "^64 bytes from 127.0.0.1: icmp_seq=[0-2] ttl=64 time=[0-9]* ms$" /tmp/p', stdout='3\n')
    c('grep "^PING" /tmp/p', stdout='PING 127.0.0.1: 56 data bytes\n')
    c('grep transmitted /tmp/p', stdout='3 packets transmitted, 3 packets received, 0.0% packet loss\n')
    c('grep -c "^round-trip min/avg/max = [0-9]*/[0-9]*/[0-9]* ms$" /tmp/p', stdout='1\n')
    c('/sbin/ping -n -c 2 -i 1 -s 1400 127.0.0.1 | grep -c "^1408 bytes from"', stdout='2\n')
    c('/sbin/ping -n -c 1 -s 0 127.0.0.1 | grep transmitted', stdout='1 packets transmitted, 1 packets received, 0.0% packet loss\n')
    c('/tmp/as-user 42 /sbin/ping -n -c 1 127.0.0.1 | grep transmitted',
      stdout='1 packets transmitted, 1 packets received, 0.0% packet loss\n')
    c('/sbin/ping -c 1 localhost | grep -c "bytes from localhost (127.0.0.1):"',stdout='1\n')
    c('/sbin/ping -n -c 1 localhost | grep -c "bytes from 127.0.0.1:"',stdout='1\n')
    c('/sbin/ping -c 1 no-such-host.invalid',stdout='ping: no-such-host.invalid: Unknown host\n',status=2)
    c('/usr/bin/nc localhost no-such-service < /dev/null',stdout='nc: no-such-service: Unknown host\n',status=2)
    c('/usr/bin/nc -l -w 1 discard',stdout='nc: accept: timed out\n',status=1)
    c('/usr/bin/nc -u -l -w 1 discard',stdout='nc: recvfrom: timed out\n',status=1)
    # Failures and usage.
    c('/sbin/ping -n -c 1 8.8.8.8', stdout='PING 8.8.8.8: 56 data bytes\nping: sendto: Network is unreachable\n', status=2)
    c('/sbin/ping', stdout=PING_USAGE, status=2)
    c('/sbin/ping -n -c 0 127.0.0.1', stdout=PING_USAGE, status=2)
    c('/sbin/ping -n -s 1473 127.0.0.1', stdout=PING_USAGE, status=2)
    c('/sbin/ping -n 999.1', stdout='ping: 999.1: Unknown host\n', status=2)
    # SIGINT ends an unbounded ping with its statistics.
    c('/sbin/ping -n 127.0.0.1 > /tmp/int & sleep 3; kill -2 $!; wait; grep -c transmitted /tmp/int', stdout='1\n')
    # The gateway answers through em0; an absent host does not.
    c('/sbin/ifconfig em0 10.0.2.15/24 up; /sbin/route add default 10.0.2.2')
    c('/sbin/ping -n -c 2 10.0.2.2 > /tmp/g')
    c('grep transmitted /tmp/g', stdout='2 packets transmitted, 2 packets received, 0.0% packet loss\n')
    c('/sbin/ping -n -c 2 -W 2 10.0.2.77 > /tmp/a', status=1)
    c('grep transmitted /tmp/a', stdout='2 packets transmitted, 0 packets received, 100.0% packet loss\n')

    # nc: TCP each way, a binary round trip, UDP.
    c('/usr/bin/nc -l 7000 < /dev/null > /tmp/t & sleep 1; echo hello | /usr/bin/nc 127.0.0.1 7000; wait; cat /tmp/t',
      stdout='hello\n')
    c('echo reply | /usr/bin/nc -l 7001 > /dev/null & sleep 1; /usr/bin/nc 127.0.0.1 7001 < /dev/null; wait', stdout='reply\n')
    c('/usr/bin/nc -l 7002 < /dev/null > /tmp/copy & sleep 1; /usr/bin/nc 127.0.0.1 7002 < /bin/sh; wait; cmp /bin/sh /tmp/copy')
    c('/usr/bin/nc -u -l -w 2 7003 < /dev/null > /tmp/u & sleep 1; echo dgram | /usr/bin/nc -u -w 1 127.0.0.1 7003; wait; cat /tmp/u',
      stdout='dgram\n')
    c('echo back | /usr/bin/nc -u -l -w 2 7004 > /dev/null & sleep 1; echo ask | /usr/bin/nc -u -w 2 127.0.0.1 7004; wait',
      stdout='back\n')
    # Failures, timeouts and usage.
    c('/usr/bin/nc 127.0.0.1 1 < /dev/null', stdout='nc: connect: Connection refused\n', status=1)
    c('/usr/bin/nc -l -w 1 7005', stdout='nc: accept: timed out\n', status=1)
    c('/usr/bin/nc -w 1 10.0.2.77 80 < /dev/null', stdout='nc: connect: timed out\n', status=1)
    c('/usr/bin/nc', stdout=NC_USAGE, status=2)
    c('/usr/bin/nc -l 1 2', stdout=NC_USAGE, status=2)
    c('/usr/bin/nc 127.0.0.1 99999', stdout='nc: bad port 99999\n', status=2)
    c('/usr/bin/nc 999.1 80', stdout='nc: 999.1: Unknown host\n', status=2)
    c('/tmp/as-user 42 /usr/bin/nc -l 80', stdout='nc: bind: Permission denied\n', status=1)
    # SIGTERM ends a listener and frees its port.
    g.command("/bin/sh -c '/usr/bin/nc -l 7006 < /dev/null & sleep 1; kill $!; wait' 2> /tmp/term", status=143)   # 128 + SIGTERM
    c('grep -c Terminated /tmp/term', stdout='1\n')   # it died of the signal
    c('/usr/bin/nc -l -w 1 7006', stdout='nc: accept: timed out\n', status=1)   # and its port is free
print(f'PASS: {ram} ping/nc guest acceptance')
