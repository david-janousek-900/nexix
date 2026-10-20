"""Real serial and VGA/PS2 visual editing, undo/search and read-only view."""
import sys,tempfile
from pathlib import Path
from private_guest import private_guest
from vga_smoke import LocalConsole
qemu,iso,ram=sys.argv[1:4]
long_entry='long:co#80:li#24:am:k0='+('a'*1500)+r':cl=\E[H\E[2J:cm=\E[%i%d;%dH:'
Path('build/vi-longterm').write_text("TERM=long\nTERMCAP='"+long_entry+"'\nexport TERM TERMCAP\n")
with tempfile.TemporaryDirectory(prefix='nx-vi-') as temp:
 with private_guest(qemu,iso,ram,'vi',{'/tmp/longterm':'vi-longterm'},extra_args=['-vga','std','-qmp',f'unix:{temp}/qmp,server=on,wait=off']) as guest:
  screen=LocalConsole(guest,temp)
  try:
   guest.expect(r'root@nexix.localdomain# ')
   guest.send('/bin/vi /tmp/serial-edit\n');before,_=guest.expect(r'\[New file\]');assert b'Using open mode' not in before
   guest.send('ialpha\nbeta\x1b');guest.expect('beta')
   guest.send('1G0xu/alpha\n:wq\n')
   guest.expect(r'root@nexix.localdomain# ')
   guest.command('cat /tmp/serial-edit',stdout='alpha\nbeta\n')
   guest.send('. /tmp/longterm; /bin/vi /tmp/long-edit\n')
   before,_=guest.expect(r'\[New file\]');assert b'Using open mode' not in before
   guest.send('ilongcap\x1b:wq\n');guest.expect(r'root@nexix.localdomain# ')
   guest.command('cat /tmp/long-edit',stdout='longcap\n')
   guest.command('TERM=ansi; TERMCAP=; export TERM TERMCAP')
   guest.send('TERM=nexix /bin/vi /tmp/vga-edit < /dev/tty0 > /dev/tty0 2>&1; echo VI_VGA_DONE\n')
   frame=screen.expect('[New file]');assert 'Using open mode' not in frame
   screen.type('ione\ntwo\x1b');screen.expect('two')
   screen.type('1G0xu/two\n:wq\n')
   guest.expect('VI_VGA_DONE\r\n');guest.expect(r'root@nexix.localdomain# ')
   guest.command('cat /tmp/vga-edit',stdout='one\ntwo\n')
   guest.send('/bin/view /tmp/serial-edit\n');guest.expect(r'\[Read only\]')
   guest.send('ix\x1b:w\n');guest.expect('[Rr]ead.only')
   guest.send(':q!\n');guest.expect(r'root@nexix.localdomain# ')
   guest.command('cat /tmp/serial-edit',stdout='alpha\nbeta\n')
   guest.send('/bin/vi /tmp/interrupt-edit\n');guest.expect(r'\[New file\]')
   guest.send('ipartial\x1b');guest.expect('partial')
   guest.send(b'\x03');guest.expect(r'\x07|Interrupt')
   guest.send(':q!\n');guest.expect(r'root@nexix.localdomain# ')
   guest.command('echo modes-restored',stdout='modes-restored\n')
  finally:screen.close()
print(f'PASS: {ram} serial and real VGA/PS2 vi editing, movement, undo/search/save, view and restoration')
