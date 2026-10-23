"""Interface and route configuration through the installed commands (task 427)."""
import sys
from private_guest import private_guest
qemu, iso, ram, nic = sys.argv[1:5]
extra = ('-nic', 'user,model=e1000') if nic == 'e1000' else ('-nic', 'none')
USAGE_IF = ('usage: ifconfig [-a]\n       ifconfig interface\n'
            '       ifconfig interface [address[/bits]] [netmask mask] [up|down]\n'
            '       ifconfig interface delete [up|down]\n')
with private_guest(qemu, iso, ram, 'netconfig-' + nic, {'/tmp/netconfig': 'netconfig-api.elf'}, extra_args=extra) as g:
    g.expect(r'root@[A-Za-z0-9.-]+# ')
    c = g.command
    # The first socket starts the stack: lo0 is up at 127.0.0.1/8.
    c('/sbin/ifconfig lo0 | grep flags', stdout='lo0: flags=49<UP,LOOPBACK,RUNNING> mtu 16384\n')
    c('/sbin/ifconfig lo0 | grep inet', stdout='\tinet 127.0.0.1 netmask 255.0.0.0\n')
    # Every line is complete: the counters line ends in a newline before the prompt.
    c('/sbin/ifconfig lo0 | grep -c "^	*packets in [0-9]* out [0-9]*, errors in 0 out 0, drops 0$"', stdout='1\n')
    c('/sbin/ifconfig | grep -c flags', stdout='2\n' if nic == 'e1000' else '1\n')
    c('/sbin/ifconfig | grep -c "^	*packets in"', stdout='2\n' if nic == 'e1000' else '1\n')
    c('/sbin/route', stdout='Destination     Gateway         Netmask         Interface\n'
                           '127.0.0.0       link            255.0.0.0       lo0\n')
    c('/sbin/route get 127.1.2.3', stdout='127.1.2.3 dev lo0 src 127.0.0.1\n')
    # Malformed input is refused before anything changes.
    c('/sbin/ifconfig lo0 1.2.3', stdout='ifconfig: bad address 1.2.3\n', status=2)
    c('/sbin/ifconfig lo0 127.0.0.1/33', stdout='ifconfig: bad prefix /33\n', status=2)
    c('/sbin/ifconfig lo0 127.0.0.1 netmask 255.0.255.0', stdout='ifconfig: lo0: Invalid argument\n', status=1)
    c('/sbin/ifconfig lo0 up down', stdout=USAGE_IF, status=2)
    c('/sbin/ifconfig lo0 netmask 255.0.0.0', stdout=USAGE_IF, status=2)
    c('/sbin/ifconfig em9 up', stdout='ifconfig: em9: No such device or address\n', status=1)
    c('/sbin/route add default 10.0.2.2', stdout='route: 10.0.2.2: Network is unreachable\n', status=1)
    c('/sbin/route add default 1.2.3.x', stdout='route: bad gateway 1.2.3.x\n', status=2)
    c('/sbin/route get 224.0.0.1', stdout='route: 224.0.0.1: Invalid argument\n', status=1)
    c('/sbin/route get 8.8.8.8', stdout='route: 8.8.8.8: Network is unreachable\n', status=1)
    if nic == 'e1000':
        c('/sbin/ifconfig em0 | grep flags', stdout='em0: flags=2<BROADCAST> mtu 1500\n')
        c('/sbin/ifconfig em0 10.0.2.255/24 up', stdout='ifconfig: em0: Invalid argument\n', status=1)
        c('/sbin/ifconfig em0 | grep flags', stdout='em0: flags=2<BROADCAST> mtu 1500\n')   # nothing applied
        c('/sbin/ifconfig em0 127.0.0.2/24', stdout='ifconfig: em0: Invalid argument\n', status=1)
        c('/sbin/ifconfig em0 127.0.0.2 netmask 0xffffff00', stdout='ifconfig: em0: Invalid argument\n', status=1)
        c('/sbin/ifconfig em0 10.0.2.15/24 up')
        c('/sbin/ifconfig em0 | grep flags', stdout='em0: flags=43<UP,BROADCAST,RUNNING> mtu 1500\n')
        c('/sbin/ifconfig em0 | grep ether', stdout='\tether 52:54:00:12:34:56\n')
        c('/sbin/ifconfig em0 | grep inet', stdout='\tinet 10.0.2.15 netmask 255.255.255.0 broadcast 10.0.2.255\n')
        c('/sbin/ifconfig em0 10.1.0.1/8', stdout='')   # readdress: classless prefix
        c('/sbin/ifconfig em0 10.0.2.15', stdout='')    # classful default: /8 for 10/8
        c('/sbin/ifconfig em0 | grep inet', stdout='\tinet 10.0.2.15 netmask 255.0.0.0 broadcast 10.255.255.255\n')
        c('/sbin/ifconfig em0 10.0.2.15 netmask 255.255.255.0')
        c('/sbin/route add default 10.0.2.15', stdout='route: 10.0.2.15: Invalid argument\n', status=1)
        c('/sbin/route add default 10.0.2.2')
        c('/sbin/route', stdout='Destination     Gateway         Netmask         Interface\n'
                               '127.0.0.0       link            255.0.0.0       lo0\n'
                               '10.0.2.0        link            255.255.255.0   em0\n'
                               'default         10.0.2.2        0.0.0.0         em0\n')
        c('/sbin/route get 8.8.8.8', stdout='8.8.8.8 via 10.0.2.2 dev em0 src 10.0.2.15\n')
        c('/sbin/route get 10.0.2.3', stdout='10.0.2.3 dev em0 src 10.0.2.15\n')
        c('/sbin/route get 10.0.2.15', stdout='10.0.2.15 dev lo0 src 10.0.2.15 local\n')
        c('/sbin/route get 10.0.2.255', stdout='10.0.2.255 dev em0 src 10.0.2.15 broadcast\n')
        c('/tmp/netconfig', stdout='NETCONFIG_API_OK\n')
        # Down keeps the address but stops routing; delete removes it.
        c('/sbin/ifconfig em0 down')
        c('/sbin/route', stdout='Destination     Gateway         Netmask         Interface\n'
                               '127.0.0.0       link            255.0.0.0       lo0\n'
                               'default         10.0.2.2        0.0.0.0         -\n')
        c('/sbin/route get 8.8.8.8', stdout='route: 8.8.8.8: Network is unreachable\n', status=1)
        c('/sbin/ifconfig em0 | grep inet', stdout='\tinet 10.0.2.15 netmask 255.255.255.0 broadcast 10.0.2.255\n')
        c('/sbin/ifconfig em0 delete up')
        c('/sbin/ifconfig em0 | grep -c inet', stdout='0\n', status=1)
        c('/sbin/route delete default')
        c('/sbin/route', stdout='Destination     Gateway         Netmask         Interface\n'
                               '127.0.0.0       link            255.0.0.0       lo0\n')
    else:
        c('/sbin/ifconfig em0 up', stdout='ifconfig: em0: No such device or address\n', status=1)
        c('/tmp/netconfig none', stdout='NETCONFIG_API_OK\n')
print(f'PASS: {ram} {nic} interface/route configuration guest acceptance')
