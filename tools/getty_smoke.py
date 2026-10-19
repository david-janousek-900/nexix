"""Check installed getty prompts/editing and a private login exec boundary."""
import re
import sys
from private_guest import private_guest


def main():
    qemu,iso,ram=sys.argv[1:4]
    with private_guest(qemu,iso,ram,'getty',{'/bin/login':'getty-handoff.elf'},2) as guest:
        guest.expect(r'(?s)BOOT OK\r\nroot@[A-Za-z0-9.-]+ # ')
        def handoff(text,terminal=0,status=0,cycle=False,profile='9'):
            redirects=' </dev/ttyS1 >/dev/ttyS1 2>/dev/ttyS1' if terminal else ''
            line="/etc/getty "+profile+redirects+"; R=$?; stty 115200 -raw echo -lcase -nl erase '^?' kill '^U'; echo GETTY_STATUS_$R"
            guest.send(line+'\n');guest.expect(re.escape(line+'\r\n'))
            prompt=b'login: '
            if profile!='9':prompt=bytes(c|((c.bit_count()&1)<<7) for c in prompt)
            guest.expect(re.escape(prompt),terminal)
            if cycle:guest.send(b'\0',terminal);guest.expect('login: ',terminal)
            guest.send(text,terminal)
            if status==0:guest.expect('GETTY_HANDOFF_OK\r\n',terminal)
            guest.expect(f'GETTY_STATUS_{status}\r\nroot@[A-Za-z0-9.-]+ # ')
        for text in ('alice\n','mist\x15alix\x7fce\r','alix\x08ce\r','ALICE\n','abcdefghijklmnop\n','\n'):
            handoff(text)
        handoff('alice\n',cycle=True)
        handoff('\x04',status=1)
        handoff('alice\n',terminal=1)
        handoff('alice\n',profile='2')
        guest.command('echo GETTY_PARENT_ALIVE',stdout='GETTY_PARENT_ALIVE\n')
    print(f'PASS: {ram} getty clean native/historical parity prompts, editing/case/length, speed cycle, EOT and cooked login exec handoff on both ports')


if __name__=='__main__':main()
