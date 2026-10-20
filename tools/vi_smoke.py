"""Real serial and VGA/PS2 visual editing, undo/search and read-only view."""
import sys,tempfile
from private_guest import private_guest
from vga_smoke import LocalConsole
qemu,iso,ram=sys.argv[1:4]
with tempfile.TemporaryDirectory(prefix='nx-vi-') as temp:
 with private_guest(qemu,iso,ram,'vi',{'/tmp/vi':'vi-dev.elf','/tmp/view':'vi-dev.elf'},extra_args=['-vga','std','-qmp',f'unix:{temp}/qmp,server=on,wait=off']) as guest:
  screen=LocalConsole(guest,temp)
  try:
   guest.expect(r'root@nexix.localdomain# ')
   guest.send('/tmp/vi /tmp/serial-edit\n');guest.expect(r'\[New file\]')
   guest.send('ialpha\nbeta\x1b');guest.expect('beta')
   guest.send('1G0xu/alpha\n:wq\n')
   guest.expect(r'root@nexix.localdomain# ')
   guest.command('cat /tmp/serial-edit',stdout='alpha\nbeta\n')
   guest.send('TERM=nexix /tmp/vi /tmp/vga-edit < /dev/tty0 > /dev/tty0 2>&1; echo VI_VGA_DONE\n')
   screen.expect('[New file]')
   screen.type('ione\ntwo\x1b');screen.expect('two')
   screen.type('1G0xu/two\n:wq\n')
   guest.expect('VI_VGA_DONE\r\n');guest.expect(r'root@nexix.localdomain# ')
   guest.command('cat /tmp/vga-edit',stdout='one\ntwo\n')
   guest.send('/tmp/view /tmp/serial-edit\n');guest.expect(r'\[Read only\]')
   guest.send('ix\x1b:w\n');guest.expect('[Rr]ead.only')
   guest.send(':q!\n');guest.expect(r'root@nexix.localdomain# ')
   guest.command('cat /tmp/serial-edit',stdout='alpha\nbeta\n')
   guest.command('echo modes-restored',stdout='modes-restored\n')
  finally:screen.close()
print(f'PASS: {ram} serial and real VGA/PS2 vi editing, movement, undo/search/save, view and restoration')
