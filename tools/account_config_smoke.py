"""Check installed development accounts, credentials and home permissions."""
import sys
from private_guest import private_guest


def main():
    qemu,iso,ram=sys.argv[1:4]
    with private_guest(qemu,iso,ram,'account-config',{'/tmp/accounts':'account-config.elf'}) as guest:
        guest.expect(r'(?s)BOOT OK\r\nroot@[A-Za-z0-9.-]+# ')
        guest.command('/tmp/accounts',stdout='ACCOUNT_CONFIG_OK\n')
    print(f'PASS: {ram} installed accounts, DES passwords, group records, credential transitions and home/configuration permissions')


if __name__=='__main__':main()
