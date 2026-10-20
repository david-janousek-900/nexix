"""Rendered paging, search and termios restoration on serial and VGA."""
from pathlib import Path
import sys,tempfile,time
from private_guest import private_guest
from vga_smoke import LocalConsole
from terminal_screen import TerminalScreen
qemu,iso,ram=sys.argv[1:4]
Path('build/pager-input').write_text(''.join(f'line {n:03d} alpha\n' for n in range(1,101)))
Path('build/pager-tabs').write_bytes(b'\tA\bB\n\033[2Jliteral\n'+b'X'*81+b'\n')
with tempfile.TemporaryDirectory(prefix='nx-pager-') as temp:
 with private_guest(qemu,iso,ram,'pager',{'/tmp/more':'v7-more.elf','/tmp/input':'pager-input','/tmp/tabs':'pager-tabs','/tmp/manfmt':'v7-manfmt.elf'},extra_args=['-vga','std','-qmp',f'unix:{temp}/qmp,server=on,wait=off']) as g:
  screen=LocalConsole(g,temp)
  try:
   g.expect(r'root@nexix.localdomain# ')
   g.command('/tmp/more /tmp/input > /tmp/copy && cmp /tmp/input /tmp/copy')
   g.command('/tmp/more /tmp/absent',stdout='more: No such file or directory\n',status=1)
   for backend in ['serial','vga']:
    observer=TerminalScreen();offset=[len(g.buffers[0])];height=24 if backend=='serial' else 25;page=height-1
    command='TERM=ansi /tmp/more /tmp/input' if backend=='serial' else 'TERM=nexix /tmp/more /tmp/input < /dev/tty0 > /dev/tty0'
    g.send(command+'; echo PAGER_DONE\n')
    def send(keys):
     if backend=='serial':g.send(keys)
     else:screen.type(keys)
    def check(first):
     deadline=time.monotonic()+4
     while True:
      g.pump(.05)
      if backend=='serial':observer.feed(g.buffers[0][offset[0]:]);offset[0]=len(g.buffers[0]);rows=observer.screen()
      else:rows=screen.screen().split('\n')
      if rows[0]==f'line {first:03d} alpha' and rows[-1].startswith('--More-- '):return
      if time.monotonic()>deadline:raise AssertionError((backend,first,rows))
    check(1)
    for keys,first in [(' ',page+1),('b',1),('j',2),('k',1),('G',101-page),('g',1),('/line 050\n',50),('?',50)]:
     if keys=='?':break
     send(keys);check(first)
    send('?line 020\n');check(20)
    send('n');check(20) # No preceding occurrence: stable position.
    send('N');check(20)
    send('g');check(1)
    if backend=='serial':send('\x1b[6~');check(page+1);send('\x1b[5~');check(1);send('\x1b[B');check(2);send('\x1b[A');check(1)
    send('q');g.expect('PAGER_DONE\r\n');g.expect(r'root@nexix.localdomain# ')
   # A long/tabbed document cannot overwrite input while normalization expands it.
   g.send('TERM=ansi /tmp/more /tmp/tabs; echo PAGER_DONE\n');g.expect('--More-- ');g.send('q');g.expect('PAGER_DONE\r\n');g.expect(r'root@nexix.localdomain# ')
   g.command('echo terminal-restored',stdout='terminal-restored\n')
   g.command('MANPAGER=/bin/cat /tmp/manfmt /tmp/absent',stdout='man: formatter: No such file or directory\n',status=127)
   g.command('MANPAGER=/tmp/absent /tmp/manfmt /bin/echo hello',stdout='man: pager: No such file or directory\n',status=127)
   g.send('MANPAGER=/tmp/more /tmp/manfmt /bin/echo Pager-fixture; echo HELPER_DONE_$?\n')
   g.expect('--More-- ');g.send('q');g.expect('HELPER_DONE_0\r\n');g.expect(r'root@nexix.localdomain# ')
   g.send('TERM=ansi /tmp/more /tmp/input; echo SIGNAL_DONE_$?\n');g.expect('--More-- ')
   g.send('\x03');g.expect('SIGNAL_DONE_130\r\n');g.expect(r'root@nexix.localdomain# ')
   g.command('echo signal-restored',stdout='signal-restored\n')
  finally:screen.close()
print('PASS: '+ram+' serial/VGA pager movement, search, arrows, literal controls, redirection and terminal restoration')
