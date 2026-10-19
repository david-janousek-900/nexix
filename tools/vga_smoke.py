"""Exercise a production VGA login with QMP keys and physical VGA observations."""
import json
from pathlib import Path
import re
import shutil
import socket
import sys
import tempfile
import time
from guest_acceptance import Guest
from multiuser_acceptance import login

class LocalConsole:
    def __init__(self, guest, directory):
        self.guest=guest
        self.path=Path(directory)/'screen.bin'
        self.sock=socket.socket(socket.AF_UNIX)
        deadline=time.monotonic()+10
        while True:
            try:self.sock.connect(str(Path(directory)/'qmp'));break
            except (FileNotFoundError,ConnectionRefusedError):
                if time.monotonic()>deadline:raise
                guest.pump(.05)
        self.sock.settimeout(10)
        self.stream=self.sock.makefile('rwb',buffering=0)
        json.loads(self.stream.readline())
        self.call('qmp_capabilities')
    def call(self, command, **arguments):
        self.stream.write((json.dumps({'execute':command,'arguments':arguments})+'\n').encode())
        while True:
            reply=json.loads(self.stream.readline())
            if 'error' in reply:raise RuntimeError(reply['error'])
            if 'return' in reply:return reply['return']
    def screen(self):
        self.call('pmemsave',val=0xb8000,size=4000,filename=str(self.path))
        data=self.path.read_bytes()[::2]
        return '\n'.join(data[i:i+80].decode('ascii',errors='replace').rstrip() for i in range(0,2000,80))
    def expect(self,text,timeout=20):
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            screen=self.screen()
            if text.rstrip() in screen:return screen
            self.guest.pump(.05)
        raise AssertionError(f'VGA missing {text!r}:\n{self.screen()}')
    def key(self,*keys):
        self.call('send-key',keys=[{'type':'qcode','data':k} for k in keys],**{'hold-time':20})
        self.guest.pump(.03)
    def type(self,text):
        names={' ':'spc','\n':'ret','\r':'ret','\x7f':'backspace','-':'minus','=':'equal','/':'slash','.':'dot',',':'comma',';':'semicolon',"'":'apostrophe','[':'bracket_left',']':'bracket_right','\\':'backslash','`':'grave_accent'}
        shifts={'_':'minus',':':'semicolon','>':'dot','<':'comma','?':'slash','"':'apostrophe','|':'backslash','!':'1','@':'2','#':'3','$':'4','%':'5','^':'6','&':'7','*':'8','(':'9',')':'0','+':'equal','{':'bracket_left','}':'bracket_right','~':'grave_accent'}
        for c in text:
            if c in shifts:self.key('shift',shifts[c])
            elif c.isupper():self.key('shift',c.lower())
            elif ord(c)<32 and c not in '\r\n':self.key('ctrl',chr(ord(c)+96))
            else:self.key(names.get(c,c))
    def close(self):self.stream.close();self.sock.close()

def main():
    qemu,iso,ram=sys.argv[1:4]
    with tempfile.TemporaryDirectory(prefix='nx-vga-') as temp:
        disk=Path(temp)/'root.img';shutil.copyfile('build/distribution.img',disk)
        guest=Guest(qemu,iso,disk,ram,2,extra_args=['-vga','std','-qmp',f'unix:{temp}/qmp,server=on,wait=off'])
        console=None
        try:
            console=LocalConsole(guest,temp)
            console.expect('login: ')
            console.type('roox\x7ft\n');console.expect('login: root\nPassword:')
            console.type('nexix\n');console.expect('# ')
            assert not re.search(r'(?:Password:\s*|\n)nexix(?:\s*\n|$)',console.screen()), 'VGA password echoed'
            console.type('hostname gui.example.test\n');console.expect('root@gui.example.test# ')
            console.type('uname -n\n');console.expect('\ngui.example.test\n')
            console.type('tty\n');console.expect('/dev/tty0')
            console.type('echo VGA_WORKS\n');console.expect('\nVGA_WORKS\n')
            login(guest,'alice','alice',prompt='$ ')
            login(guest,'bob','bob',1,prompt='$ ')
            guest.command('tty',stdout='/dev/console\n',prompt=re.escape('$ '))
            guest.command('tty',1,stdout='/dev/ttyS1\n',prompt=re.escape('$ '))
            console.type('echo EDIx\x7fT_OK\n');console.expect('\nEDIT_OK\n')
            console.type('discard this\x15echo KILL_OK\n');console.expect('\nKILL_OK\n')
            console.type('cat\n');console.type('\x03');console.type('echo INTERRUPT_OK\n');console.expect('\nINTERRUPT_OK\n')
            guest.command('echo SERIAL_ALIVE',stdout='SERIAL_ALIVE\n',prompt=re.escape('$ '))
            assert 'SERIAL_ALIVE' not in console.screen()
            console.type('who\n');screen=console.expect('root     tty0')
            assert 'alice    console' in screen and 'bob      ttyS1' in screen
            console.type('\x04');console.expect('login: ')
            console.type('alice\n');console.expect('Password:')
            console.type('alice\n');console.expect('$ ')
            console.type('echo RELOGIN_OK\n');console.expect('\nRELOGIN_OK\n')
            print(f'PASS: {ram} VGA hostname/prompt, login, hidden password, tty identity, editing, signals, logout/relogin and three-session isolation')
        finally:
            try:
                if console:Path(f'build/vga-{ram}.screen.txt').write_text(console.screen())
            finally:
                if console:console.close()
                for i,output in enumerate(guest.buffers):Path(f'build/vga-{ram}.tty{i}.log').write_bytes(output)
                guest.diagnostics.seek(0)
                Path(f'build/vga-{ram}.qemu.log').write_bytes(guest.diagnostics.read())
                guest.close()

if __name__=='__main__':main()
