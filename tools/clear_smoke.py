"""Exercise the installed clear client on serial and the real VGA device."""
import sys,tempfile
from private_guest import private_guest
from vga_smoke import LocalConsole
qemu,iso,ram=sys.argv[1:4]
with tempfile.TemporaryDirectory(prefix='nx-clear-') as temp:
 with private_guest(qemu,iso,ram,'clear',{},extra_args=['-vga','std','-qmp',f'unix:{temp}/qmp,server=on,wait=off']) as guest:
  screen=LocalConsole(guest,temp)
  try:
   guest.expect(r'root@nexix.localdomain# ')
   guest.command('clear && echo',stdout='\033[H\033[2J\n')
   guest.command('TERM=ansi clear && echo',stdout='\033[H\033[2J\n')
   guest.command('TERM=nexix clear && echo',stdout='\033[H\033[2J\n')
   guest.command('TERM= clear',stdout='clear: TERM is not set\n',status=1)
   guest.command('TERM=unknown clear',stdout='clear: terminal description unavailable\n',status=1)
   guest.command("TERM=dumb TERMCAP='dumb:co#80:' clear",stdout='clear: terminal cannot clear the screen\n',status=1)
   guest.command('clear extra',stdout='usage: clear\n',status=2)
   guest.command('TERM=ansi clear 1>&-',stdout='clear: output error\n',status=1)
   guest.command('echo OLD_SCREEN_TEXT > /dev/tty0')
   screen.expect('OLD_SCREEN_TEXT')
   guest.command('TERM=nexix clear > /dev/tty0')
   screen.screen()
   raw=screen.path.read_bytes()
   assert all(raw[i]==32 for i in range(0,4000,2)), 'clear left characters on VGA'
  finally:screen.close()
print(f'PASS: {ram} installed clear serial bytes, VGA screen and errors')
