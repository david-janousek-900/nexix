"""Installed od versus an independent byte-format oracle."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tests'))
from od import expected
qemu,iso,ram=sys.argv[1:4]
data=bytes(range(256));Path('build/od-bytes').write_bytes(data)
repeat=b'\0'*49;Path('build/od-repeat').write_bytes(repeat)
with private_guest(qemu,iso,ram,'od',{'/tmp/bytes':'od-bytes','/tmp/repeat':'od-repeat'},single_user=False) as guest:
    login(guest,'root','nexix')
    for formats in ['o','d','x','c','b','odxcb']:
        guest.command('od -'+formats+' /tmp/bytes',stdout=expected(data,formats))
    guest.command('cat /tmp/bytes | od',stdout=expected(data))
    guest.command('od /tmp/repeat',stdout=expected(repeat))
    guest.command('od /tmp/bytes +10.',stdout=expected(data[10:],'o',10,10))
    guest.command('od /tmp/bytes +x10',stdout=expected(data[16:],'o',16,16))
    guest.command('od -z /tmp/bytes',stdout='od: unknown format\n',status=1)
    guest.command('od /tmp/missing',stdout='cannot open /tmp/missing\n',status=1)
    guest.command('od /tmp/bytes +9999999999999999999999.',stdout='od: invalid offset or seek error\n',status=1)
print(f'PASS: {ram} od all V7 formats, binary fixtures, duplicate/partial rows, stdin, offsets and errors')
