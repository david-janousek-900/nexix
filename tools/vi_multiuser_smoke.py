"""Three concurrent installed editors on real VGA and independent serial ttys."""
from pathlib import Path
import shutil,sys,tempfile
from guest_acceptance import Guest
from multiuser_acceptance import login
from vga_smoke import LocalConsole
qemu,iso,ram=sys.argv[1:4]
with tempfile.TemporaryDirectory(prefix='nx-vi-multi-') as temp:
 disk=Path(temp)/'root.img';shutil.copyfile('build/distribution.img',disk)
 guest=Guest(qemu,iso,disk,ram,2,extra_args=['-vga','std','-qmp',f'unix:{temp}/qmp,server=on,wait=off'])
 screen=None;third=ram!='16M'
 try:
  screen=LocalConsole(guest,temp)
  login(guest,'alice','alice',prompt='$ ');login(guest,'bob','bob',1,prompt='$ ')
  screen.expect('login: ');screen.type('root\n');screen.expect('Password:');screen.type('nexix\n');screen.expect('# ')
  guest.send('vi /tmp/alice-vi\n');guest.expect(r'\[New file\]')
  if third:guest.send('vi /tmp/bob-vi\n',1);guest.expect(r'\[New file\]',1)
  screen.type('vi /tmp/root-vi\n');screen.expect('[New file]')
  guest.send('iALICE_EDIT\x1b');guest.expect('ALICE_EDIT')
  if third:guest.send('iBOB_EDIT\x1b',1);guest.expect('BOB_EDIT',1)
  screen.type('iROOT_EDIT\x1b');frame=screen.expect('ROOT_EDIT')
  assert 'ALICE_EDIT' not in frame and 'BOB_EDIT' not in frame
  assert b'BOB_EDIT' not in guest.buffers[0] and b'ROOT_EDIT' not in guest.buffers[0]
  assert b'ALICE_EDIT' not in guest.buffers[1] and b'ROOT_EDIT' not in guest.buffers[1]
  if third:
   guest.send(':wq\n',1);guest.expect(r'\$ ',1)
   guest.command('cat /tmp/bob-vi',1,stdout='BOB_EDIT\n')
  guest.send(':wq\n');guest.expect(r'\$ ')
  guest.command('cat /tmp/alice-vi',stdout='ALICE_EDIT\n')
  screen.type(':wq\n');screen.expect('# ')
  screen.type('cat /tmp/root-vi\n');screen.expect('\nROOT_EDIT\n')
  guest.command('echo serial-restored',stdout='serial-restored\n')
  guest.command('echo second-restored',1,stdout='second-restored\n')
 finally:
  if screen:
   Path(f'build/vi-multiuser-{ram}.screen.txt').write_text(screen.screen());screen.close()
  for n,data in enumerate(guest.buffers):Path(f'build/vi-multiuser-{ram}.tty{n}.log').write_bytes(data)
  guest.close()
print(f'PASS: {ram} concurrent installed VGA/serial vi sessions, independent display/input/save and restored ttys')
