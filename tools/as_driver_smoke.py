"""Installed assembler pipeline, byte oracle and linked execution in native guest."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
f=Path('build/fixtures-as');(f/'bad.s').write_text('.bss\n.byte 1\n')
artifacts={'/tmp/object.s':'fixtures-as/object.s','/tmp/part1.s':'fixtures-as/part1.s',
 '/tmp/part2.s':'fixtures-as/part2.s','/tmp/expected.o':'fixtures-as/object.o',
 '/tmp/bad.s':'fixtures-as/bad.s','/tmp/objrun':'as-object-runtime.elf'}
with private_guest(qemu,iso,ram,'as-driver',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('as -o /tmp/output.o /tmp/part1.s /tmp/part2.s')
    guest.command('cmp /tmp/output.o /tmp/expected.o')
    guest.command('as -u -o /tmp/stdin.o < /tmp/object.s')
    guest.command('cmp /tmp/stdin.o /tmp/expected.o')
    guest.command('/lib/as2 -o /tmp/helper.o /tmp/object.s')
    guest.command('cmp /tmp/helper.o /tmp/expected.o')
    guest.command("nm -g /tmp/output.o | grep -c ' T native_function$'",stdout='1\n')
    guest.command('as -o /tmp/output.o /tmp/bad.s',stdout='as: invalid layout, symbol or relocation\n',status=1)
    guest.command('cmp /tmp/output.o /tmp/expected.o')
    guest.command('/tmp/objrun')
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('as -o /tmp/bob.o /tmp/object.s',terminal=1)
    guest.command('cmp /tmp/bob.o /tmp/expected.o',terminal=1)
    guest.command('/tmp/objrun',terminal=1)
print(f'PASS: {ram} installed as/as2, multiple inputs, stdin, exact object bytes, consumers, ordinary user and execution')
