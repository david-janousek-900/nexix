"""Exercise the installed V7 stty command against both physical terminals."""
import sys
from private_guest import private_guest


def main():
    qemu,iso,ram=sys.argv[1:4]
    with private_guest(qemu,iso,ram,'stty',{'/tmp/sttycheck':'stty-command.elf'},2) as guest:
        guest.expect(r'(?s)BOOT OK\r\n# ')
        guest.command('/tmp/sttycheck',stdout='STTY_OK\n')
    print(f'PASS: {ram} installed stty query, native baud, per-line changes, raw mode, malformed input and non-TTY errors')


if __name__=='__main__':main()
