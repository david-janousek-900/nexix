"""Guest acceptance for native kernel snapshots and the root home directory."""
import re
import sys
from pathlib import Path
from private_guest import private_guest
from multiuser_acceptance import login


def main():
    qemu,iso,ram=sys.argv[1:4]
    with private_guest(qemu,iso,ram,'dmesg',{},2,single_user=False) as guest:
        login(guest,'root','nexix')
        lines=bytes(guest.buffers[0]).replace(b'\r',b'').decode().splitlines()
        kernel=[line for line in lines if re.match(r'^\[ *\d+\.\d{6}\] ',line)]
        assert kernel and 'x86_64 kernel entered' in kernel[0],kernel
        values=[float(re.match(r'^\[ *(\d+\.\d{6})\]',line)[1]) for line in kernel]
        assert values==sorted(values) and values[0]==0,values
        expected='\n'.join(kernel)+'\n'
        assert 'NEXIX_' not in expected and 'Nexix:' not in expected
        guest.command('pwd',stdout='/root\n')
        guest.command('echo $HOME',stdout='/root\n')
        guest.command('test -d /root')
        guest.command('dmesg',stdout=expected)
        guest.command('dmesg',stdout=expected)
        guest.command('dmesg -',stdout=expected)
        guest.command('dmesg -')
        guest.command('dmesg',stdout=expected)
        guest.command('dmesg --bad',stdout='usage: dmesg [-]\n',status=2)
        guest.command('dmesg /dev/mem /unix',stdout='usage: dmesg [-]\n',status=2)
        guest.command('echo TERMINAL_ONLY',stdout='TERMINAL_ONLY\n')
        guest.command('dmesg',stdout=expected)
        login(guest,'alice','alice',1,prompt='$ ')
        guest.command('test -r /root/.profile',1,status=1,prompt=re.escape('$ '))
        guest.command('dmesg',1,stdout='dmesg: Not owner\n',status=1,prompt=re.escape('$ '))
        # Saved snapshots are per invocation state, independent of retained logs.
        guest.command("echo broken > /usr/adm/msgbuf")
        guest.command('dmesg -',stdout='...\n'+expected)
        guest.command('dmesg -')
        Path(f'build/dmesg-{ram}.log').write_bytes(guest.buffers[0])
    print(f'PASS: {ram} root home/privacy, timestamped kernel-only snapshots, repeat/incremental reads, CLI errors and denied access')


if __name__=='__main__':main()
