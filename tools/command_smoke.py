"""Run a private native state-inspection fixture against an installed command."""
import sys
import tempfile
import shutil
from pathlib import Path
from private_guest import private_guest
qemu,iso,ram,name=sys.argv[1:5]
with tempfile.TemporaryDirectory(prefix='nx-command-') as temp:
    extra=[]
    if name=='mv':
        auxiliary=Path(temp)/'aux.img';shutil.copyfile('build/root.img',auxiliary)
        with auxiliary.open('ab') as stream:stream.truncate(9614*512)
        extra=['-drive',f'file={auxiliary},format=raw,if=ide,index=1']
    with private_guest(qemu,iso,ram,name,{'/bin/check':name+'-command.elf'},extra_args=extra) as guest:
        guest.expect(r'(?s)BOOT OK\r\nroot@[A-Za-z0-9.-]+# ')
        prefix="Sending output to 'nohup.out'\n"*2 if name=='nohup' else ''
        guest.command('/bin/check',stdout=prefix+name.upper()+'_OK\n')
print(f'PASS: {ram} installed {name}: output/status, native metadata and failure fixtures')
