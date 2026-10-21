"""Installed V7 checksum against an independent 16-bit arithmetic oracle."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login

qemu, iso, ram = sys.argv[1:4]
fixtures = {'empty': b'', 'one': b'a', 'edge': b'x' * 512,
            'over': b'x' * 513, 'binary': bytes(range(256)) * 33}
artifacts = {}
for name, data in fixtures.items():
    artifact = 'sum-' + name
    Path('build', artifact).write_bytes(data)
    artifacts['/tmp/' + name] = artifact


def expected(data):
    value = 0
    for byte in data:
        value = ((value // 2) + (32768 if value % 2 else 0) + byte) % 65536
    return f'{value:05d}{(len(data) + 511) // 512:6d}'


with private_guest(qemu, iso, ram, 'sum', artifacts, single_user=False) as guest:
    login(guest, 'root', 'nexix')
    for name, data in fixtures.items():
        guest.command('sum /tmp/' + name, stdout=expected(data) + '\n')
    guest.command('cat /tmp/binary | sum', stdout=expected(fixtures['binary']) + '\n')
    guest.command('sum /tmp/one /tmp/empty', stdout=''.join(
        expected(fixtures[n]) + ' /tmp/' + n + '\n' for n in ('one', 'empty')))
    guest.command('sum /tmp/absent', stdout="sum: Can't open /tmp/absent\n", status=10)
    guest.command('sum /tmp/absent /tmp/one', stdout="sum: Can't open /tmp/absent\n" +
                  expected(fixtures['one']) + ' /tmp/one\n', status=10)
    guest.command('sum /dev/mem', stdout='sum: read error on /dev/mem\n00000     0\n', status=1)
print(f'PASS: {ram} installed sum, binary/stdin, block boundaries and error continuation')
