"""Optional /etc/netstart and the QEMU network profile on cold boots (task 429)."""
import socket
import sys
import time
from private_guest import private_guest
from multiuser_acceptance import login
qemu, iso, ram = sys.argv[1:4]
NETSTART = {'/etc/netstart': 'netstart-qemu'}   # the shipped /etc/netstart.qemu, enabled
PROMPT = r'root@[A-Za-z0-9.-]+# '

def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]

# 1. With /etc/netstart: em0 and the default route come up at boot.
with private_guest(qemu, iso, ram, 'netstart-on', NETSTART, single_user=False,
                   extra_args=('-nic', 'user,model=e1000')) as g:
    login(g, 'root', 'nexix')
    c = g.command
    c('/sbin/ifconfig em0 | grep inet', stdout='\tinet 10.0.2.15 netmask 255.255.255.0 broadcast 10.0.2.255\n',
      prompt=PROMPT)
    c('/sbin/route | grep default', stdout='default         10.0.2.2        0.0.0.0         em0\n', prompt=PROMPT)
    c('/bin/ping -c 1 10.0.2.2 | grep transmitted', stdout='1 packets transmitted, 1 packets received, 0.0% packet loss\n',
      prompt=PROMPT)

# 2. Without it (the default image): nothing beyond lo0 is configured.
with private_guest(qemu, iso, ram, 'netstart-off', {}, single_user=False,
                   extra_args=('-nic', 'user,model=e1000')) as g:
    login(g, 'root', 'nexix')
    c = g.command
    c('test -f /etc/netstart', status=1, prompt=PROMPT)
    c('test -f /etc/netstart.qemu', prompt=PROMPT)
    c('/sbin/ifconfig em0 | grep -c inet', stdout='0\n', status=1, prompt=PROMPT)
    c('/sbin/route', stdout='Destination     Gateway         Netmask         Interface\n'
                           '127.0.0.0       link            255.0.0.0       lo0\n', prompt=PROMPT)

# 3. /etc/netstart without a NIC: its failure is reported and boot goes on.
with private_guest(qemu, iso, ram, 'netstart-nonic', NETSTART, single_user=False, extra_args=('-nic', 'none')) as g:
    g.expect('ifconfig: em0: No such device or address\r\n')
    login(g, 'root', 'nexix')
    g.command('/sbin/ifconfig | grep -c flags', stdout='1\n', prompt=PROMPT)

# 4. Host forwarding: the host reaches a guest listener through QEMU.
port = free_port()
forward = f'user,model=e1000,hostfwd=tcp:127.0.0.1:{port}-:7000'
with private_guest(qemu, iso, ram, 'netstart-forward', NETSTART, single_user=False, extra_args=('-nic', forward)) as g:
    login(g, 'root', 'nexix')
    g.command("/bin/sh -c '/bin/nc -l 7000 < /dev/null > /tmp/from-host &'", prompt=PROMPT)
    time.sleep(2)
    with socket.create_connection(('127.0.0.1', port), timeout=20) as s:
        s.sendall(b'from host\n')
        s.shutdown(socket.SHUT_WR)
        assert s.recv(16) == b''   # the guest closes after end of stream
    time.sleep(1)
    g.command('cat /tmp/from-host', stdout='from host\n', prompt=PROMPT)
    g.command("/bin/sh -c 'echo to host | /bin/nc -l 7000 > /dev/null &'", prompt=PROMPT)
    time.sleep(2)
    with socket.create_connection(('127.0.0.1', port), timeout=20) as s:
        s.shutdown(socket.SHUT_WR)
        data = b''
        while chunk := s.recv(64):
            data += chunk
    assert data == b'to host\n', data
print(f'PASS: {ram} netstart on/off, no-NIC netstart and host forwarding cold boots')
