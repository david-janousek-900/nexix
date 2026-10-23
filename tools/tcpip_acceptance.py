"""TCP/IP guest acceptance (task 431): one disposable multi-user guest with an
e1000, configured by /etc/netstart, against isolated host services.

The host side listens only on 127.0.0.1, on ports chosen at run time; QEMU
user networking makes them reachable from the guest as 10.0.2.2. Nothing
leaves the host."""
import socket
import socketserver
import sys
import threading
import time
from private_guest import private_guest
from multiuser_acceptance import login
qemu, iso, ram = sys.argv[1:4]
PROMPT = r'root@[A-Za-z0-9.-]+# '


class TCPEcho(socketserver.BaseRequestHandler):
    def handle(self):
        while data := self.request.recv(65536):
            self.request.sendall(data)
        self.request.shutdown(socket.SHUT_WR)   # echo end of stream too


class UDPEcho(socketserver.BaseRequestHandler):
    def handle(self):
        data, sock = self.request
        sock.sendto(b'echo:' + data, self.client_address)


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


tcp = socketserver.ThreadingTCPServer(('127.0.0.1', 0), TCPEcho)
udp = socketserver.ThreadingUDPServer(('127.0.0.1', 0), UDPEcho)
for server in (tcp, udp):
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
tcp_port, udp_port = tcp.server_address[1], udp.server_address[1]
forward = free_port()
nic = f'user,model=e1000,hostfwd=tcp:127.0.0.1:{forward}-:7000'
try:
    with private_guest(qemu, iso, ram, 'tcpip', {'/etc/netstart': 'netstart-qemu'}, single_user=False,
                       extra_args=('-nic', nic)) as g:
        login(g, 'root', 'nexix')
        def c(line, **expect):
            expect.setdefault('prompt', PROMPT)
            background = ' & ' in line or line.endswith('&')   # run so no PID is printed
            g.command("/bin/sh -c '" + line + "'" if background else line, **expect)

        # Loopback: ICMP and TCP without any external interface involved.
        c('/bin/ping -c 2 127.0.0.1 | grep transmitted',
          stdout='2 packets transmitted, 2 packets received, 0.0% packet loss\n')
        c('/bin/nc -l 7100 < /dev/null > /tmp/lo & sleep 1; echo loop | /bin/nc 127.0.0.1 7100; wait; cat /tmp/lo',
          stdout='loop\n')
        # Static IPv4 from /etc/netstart, then ARP: the gateway resolves, an
        # absent neighbour does not.
        c('/sbin/ifconfig em0 | grep inet', stdout='\tinet 10.0.2.15 netmask 255.255.255.0 broadcast 10.0.2.255\n')
        c('/sbin/route get 192.0.2.1', stdout='192.0.2.1 via 10.0.2.2 dev em0 src 10.0.2.15\n')
        c('/bin/ping -c 3 10.0.2.2 | grep transmitted',
          stdout='3 packets transmitted, 3 packets received, 0.0% packet loss\n')
        c('/bin/ping -c 1 -W 6 10.0.2.77 | grep transmitted',
          stdout='1 packets transmitted, 0 packets received, 100.0% packet loss\n', status=1)   # ping's status
        # UDP to the host's echo service and back.
        c(f'echo datagram | /bin/nc -u -w 2 10.0.2.2 {udp_port}', stdout='echo:datagram\n')
        # Outbound TCP: a line, then a binary round trip through the host echo.
        c(f'echo outbound | /bin/nc 10.0.2.2 {tcp_port}', stdout='outbound\n')
        c(f'/bin/nc 10.0.2.2 {tcp_port} < /bin/sh > /tmp/echoed; cmp /bin/sh /tmp/echoed')
        c('/bin/nc 10.0.2.2 1 < /dev/null', stdout='nc: connect: Connection refused\n', status=1)
        # Inbound TCP through the host forward, in both directions.
        c('/bin/nc -l 7000 < /dev/null > /tmp/in &')
        time.sleep(2)
        with socket.create_connection(('127.0.0.1', forward), timeout=20) as s:
            s.sendall(b'inbound\n')
            s.shutdown(socket.SHUT_WR)
            assert s.recv(16) == b''
        time.sleep(1)
        c('cat /tmp/in', stdout='inbound\n')
        c('echo reply | /bin/nc -l 7000 > /dev/null &')
        time.sleep(2)
        with socket.create_connection(('127.0.0.1', forward), timeout=20) as s:
            s.shutdown(socket.SHUT_WR)
            data = b''
            while chunk := s.recv(64):
                data += chunk
        assert data == b'reply\n', data
        # Counters moved on em0 and nothing was dropped or failed.
        c('/sbin/ifconfig em0 | grep -c "errors in 0 out 0, drops 0"', stdout='1\n')
finally:
    tcp.shutdown(); udp.shutdown()
print(f'PASS: {ram} TCP/IP acceptance: loopback, static IPv4, ARP, ping, UDP, outbound and forwarded inbound TCP')
