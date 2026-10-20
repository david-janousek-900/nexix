"""Installed V7 man lookup/modes, modern paging and real login terminals."""
from pathlib import Path
import sys,tempfile,time
from private_guest import private_guest
from multiuser_acceptance import login
from vga_smoke import LocalConsole
from terminal_screen import TerminalScreen
qemu,iso,ram=sys.argv[1:4]
with tempfile.TemporaryDirectory(prefix='nx-man-') as temp:
 with private_guest(qemu,iso,ram,'man',{'/tmp/check':'formatter-protocol.elf'},terminals=2,single_user=False,extra_args=['-vga','std','-qmp',f'unix:{temp}/qmp,server=on,wait=off']) as g:
  screen=LocalConsole(g,temp)
  try:
   login(g,'root','nexix');login(g,'bob','bob',1,'$ ')
   for args,path in [('ls','man1/ls.1'),('2 read','man2/read.2'),('3 printf','man3/printf.3s'),('0 intro','man0/intro'),('1 ac','man1/ac.1m'),('4 tty','man4/tty.4'),('5 passwd','man5/passwd.5'),('6 checkers','man6/checkers.6'),('8 init','man8/init.8')]:
    g.command('man -w '+args,stdout=path+'\n')
   for args,pattern in [('cat','concatenate'),('2 read','read from file'),('3 printf','formatted output'),('7 nexix','native manual-reading'),('uptime','exponentially damped'),('more','screen at a time'),('vi','screen oriented'),('view','screen oriented'),('ex','ex, edit')]:
    g.command('man '+args+' > /tmp/manual')
    g.command("grep -c '"+pattern+"' /tmp/manual",stdout='1\n')
   for args in ['0 intro','0 pref','0 title','0 permindex']:
    g.command('man '+args+' > /tmp/manual')
    g.command('test -s /tmp/manual')
   g.command('man -w man',stdout='man1/man.1 man7/man.7\n')
   g.command('man -w 0 intro 1 ls',stdout='man: select section 0 separately\n',status=2)
   g.command('man no-such-page',stdout='no-such-page not found\n',status=1)
   g.command('man -w ls no-such-page',stdout='no-such-page not found\nman1/ls.1\n',status=1)
   g.command('man',stdout='usage: man [section] [-n|-ne|-t|-e|-k|-ek|-w] name ...\n',status=2)
   g.command('man -ne eqnchar > /tmp/manual')
   for args in ['-t cat','-e eqnchar']:
    g.command('man '+args+' > /tmp/format')
    g.command('/tmp/check cat /tmp/format',stdout='PROTOCOL_OK\n')
   for args in ['-k cat','-ek eqnchar']:
    g.command('man '+args+' > /tmp/format')
    g.command('/tmp/check tek /tmp/format',stdout='PROTOCOL_OK\n')
   g.command('man 2 read > /tmp/bobman',1,prompt=r'\$ ')
   g.command("grep -c 'read from file' /tmp/bobman",1,stdout='1\n',prompt=r'\$ ')
   g.send('MANPAGER=/bin/cat man cat; echo MAN_PAGER_OVERRIDE_$?\n')
   g.expect('MAN_PAGER_OVERRIDE_0\r\n');g.expect(r'# ')
   # Serial user navigation through the installed script and the producer pipe.
   g.send('man ls; echo MAN_USER_DONE_$?\n',1);g.expect('--More-- ',1)
   g.send(' bGg/SEE ALSO\n',1);g.expect('--More-- ',1)
   g.send('q',1);g.expect('MAN_USER_DONE_0\r\n',1);g.expect(r'\$ ',1)
   # Real production VGA login rather than redirected test-terminal descriptors.
   screen.expect('login: ');screen.type('root\n');screen.expect('Password:');screen.type('nexix\n');screen.expect('# ')
   screen.type('man ls\n');screen.expect('--More-- ');screen.expect('NAME')
   first=screen.screen()
   screen.type(' ');screen.expect('--More-- ')
   deadline=time.monotonic()+4
   while screen.screen()==first and time.monotonic()<deadline:g.pump(.05)
   assert screen.screen()!=first,'VGA manual did not advance'
   screen.type('b');screen.expect('NAME');screen.type('/SEE ALSO\n');screen.expect('SEE ALSO')
   screen.type('q');screen.expect('# ')
   screen.type('echo VGA_MAN_RESTORED\n');screen.expect('VGA_MAN_RESTORED\n')
   g.command('echo SERIAL_MAN_RESTORED',1,stdout='SERIAL_MAN_RESTORED\n',prompt=r'\$ ')
  finally:screen.close()
print('PASS: '+ram+' installed man sections 0–8 lookup, formatter modes, missing pages, root/user serial and VGA paging/search')
