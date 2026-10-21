"""Native ELF producer output, installed consumers and independently linked code."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
f=Path('build/fixtures-as');(f/'badobject.s').write_text('.bss\n.quad external\n')
artifacts={'/tmp/asobject':'as-object-test.elf','/tmp/object.s':'fixtures-as/object.s',
           '/tmp/expected.o':'fixtures-as/object.o','/tmp/objrun':'as-object-runtime.elf',
           '/tmp/badobject.s':'fixtures-as/badobject.s'}
with private_guest(qemu,iso,ram,'as-object',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('/tmp/asobject /tmp/object.s /tmp/output.o')
    guest.command('cmp /tmp/output.o /tmp/expected.o')
    guest.command("nm -g /tmp/output.o | grep -c ' T native_function$'",stdout='1\n')
    guest.command('size /tmp/output.o > /tmp/sizes')
    guest.command('ar rcs /tmp/objects.a /tmp/output.o')
    guest.command('ranlib /tmp/objects.a')
    guest.command("nm -go /tmp/objects.a | grep -c ' T native_function$'",stdout='1\n')
    guest.command('/tmp/objrun')
    guest.command('cp /tmp/output.o /tmp/old.o')
    guest.command('/tmp/asobject /tmp/badobject.s /tmp/output.o',stdout='as-object: cannot produce /tmp/output.o\n',status=1)
    guest.command('cmp /tmp/output.o /tmp/old.o')
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('/tmp/asobject /tmp/object.s /tmp/bob.o',terminal=1)
    guest.command('cmp /tmp/bob.o /tmp/expected.o',terminal=1)
    guest.command('/tmp/objrun',terminal=1)
print(f'PASS: {ram} native ELF production, exact bytes, nm/size/ar/ranlib consumption, linked execution and failure preservation')
