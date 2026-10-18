"""Check installed getty prompts/editing and a private login exec boundary."""
import re
import sys
from private_guest import private_guest


def main():
    qemu,iso,ram=sys.argv[1:4]
    with private_guest(qemu,iso,ram,'getty',{'/bin/login':'getty-handoff.elf'},2) as guest:
        guest.expect(r'(?s)NEXIX_BOOT_OK\r\n# ')
        def handoff(text,terminal=0,status=0,cycle=False):
            redirects=' </dev/ttyS1 >/dev/ttyS1 2>/dev/ttyS1' if terminal else ''
            line="/etc/getty 9"+redirects+"; R=$?; stty -raw echo -lcase -nl erase '^?' kill '^U'; echo GETTY_STATUS_$R"
            guest.send(line+'\n');guest.expect(re.escape(line+'\r\n'))
            guest.expect('login: ',terminal)
            if cycle:guest.send(b'\0',terminal);guest.expect('login: ',terminal)
            guest.send(text,terminal)
            if status==0:guest.expect('GETTY_HANDOFF_OK\r\n',terminal)
            guest.expect(f'GETTY_STATUS_{status}\r\n# ')
        for text in ('alice\n','mist@alix#ce\r','ALICE\n','abcdefghijklmnop\n','\n'):
            handoff(text)
        handoff('alice\n',cycle=True)
        handoff('\x04',status=1)
        handoff('alice\n',terminal=1)
        guest.command('echo GETTY_PARENT_ALIVE',stdout='GETTY_PARENT_ALIVE\n')
    print(f'PASS: {ram} getty clean prompts, editing/case/length, speed cycle, EOT and cooked login exec handoff on both ports')


if __name__=='__main__':main()
