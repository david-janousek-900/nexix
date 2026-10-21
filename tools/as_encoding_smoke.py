"""Native encoder output must match oracle-checked bytes and executable fixtures."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
f=Path('build/fixtures-as');(f/'badcode.s').write_text('movb %ah,%r8b\n')
artifacts={'/tmp/asencode':'as-encode-test.elf','/tmp/code.s':'fixtures-as/encoding.s',
           '/tmp/expected':'fixtures-as/encoding.out','/tmp/coderun':'as-code-runtime.elf','/tmp/badcode.s':'fixtures-as/badcode.s'}
with private_guest(qemu,iso,ram,'as-encode',artifacts,terminals=2,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('/tmp/asencode /tmp/code.s > /tmp/actual')
    guest.command('cmp /tmp/actual /tmp/expected')
    guest.command('/tmp/asencode /tmp/badcode.s',stdout='encode: rejected instruction on line 1\n',status=1)
    guest.command('/tmp/coderun',stdout='ENCODED_ABI_OK\n')
    login(guest,'bob','bob',line=1,prompt='$ ')
    guest.command('/tmp/asencode /tmp/code.s > /tmp/bobactual',terminal=1)
    guest.command('cmp /tmp/bobactual /tmp/expected',terminal=1)
    guest.command('/tmp/coderun',terminal=1,stdout='ENCODED_ABI_OK\n')
print(f'PASS: {ram} native x86_64 byte/relocation encoding and encoded integer/SSE System V execution')
