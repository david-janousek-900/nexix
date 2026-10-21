"""Independent free-space fixtures, corrupt chains and installed raw-device I/O."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
from mk_v7fs import Image, long, short

qemu, iso, ram = sys.argv[1:4]
image = Image(256)
image.directory(2, 2, [])
expected = image.blocks - image.next_block
good = bytes(image.finish())
fixtures = {'good': good, 'short': b'\0' * 512}
bad = bytearray(good); bad[518:520] = short(51); fixtures['count'] = bytes(bad)
bad = bytearray(good); bad[518:520] = short(1); bad[520:524] = long(256); fixtures['range'] = bytes(bad)
bad = bytearray(good); bad[518:520] = short(2); bad[520:528] = long(255) * 2; fixtures['duplicate'] = bytes(bad)
bad = bytearray(good); bad[518:520] = short(2); bad[520:528] = long(255) + long(0); fixtures['sentinel'] = bytes(bad)
bad = bytearray(good); bad[518:520] = short(1); bad[520:524] = long(255)
bad[255*512:255*512+6] = short(1) + long(255); fixtures['cycle'] = bytes(bad)
artifacts = {}
for name, data in fixtures.items():
    path = 'df-' + name; Path('build', path).write_bytes(data); artifacts['/tmp/' + name] = path
with private_guest(qemu, iso, ram, 'df', artifacts, single_user=False, terminals=2) as guest:
    login(guest, 'root', 'nexix')
    guest.command('df /tmp/good', stdout=f'/tmp/good {expected}\n')
    guest.command('df /tmp/absent /tmp/good', stdout='cannot open /tmp/absent\n' +
                  f'/tmp/good {expected}\n', status=1)
    guest.command('df /tmp/short', stdout='df: read error 1\n', status=1)
    for name in ('count', 'range', 'duplicate', 'cycle', 'sentinel'):
        guest.command('df /tmp/' + name, stdout='df: corrupt free list on /tmp/' + name + '\n', status=1)
    guest.command("df | grep -c '^/dev/root '", stdout='1\n')
    login(guest, 'bob', 'bob', 1, '$ ')
    guest.command('df /dev/root', 1, stdout='cannot open /dev/root\n', status=1, prompt=r'\$ ')
    guest.command('df /tmp/good', 1, stdout=f'/tmp/good {expected}\n', prompt=r'\$ ')
print(f'PASS: {ram} df chained free count, binary encoding, corrupt/cyclic lists, raw root and permissions')
