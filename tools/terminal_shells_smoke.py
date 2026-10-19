"""Concurrent real V7 shells on COM1/COM2, including repeated exit/recovery."""
import json
from pathlib import Path
import sys
import tempfile
from guest_acceptance import Guest
from install_manifest import MANIFEST,ROOT
from mk_rootfs import build
from mk_v7fs import write_image


def main():
    qemu,iso,ram=sys.argv[1:4]
    with tempfile.TemporaryDirectory(prefix='nx-shells-') as temp:
        plan=json.loads(MANIFEST.read_text())
        entry=next(e for e in plan['entries'] if e['path']=='/etc/init')
        entry.update(component='distribution.config',format='data',source={'root':'programs','path':'terminal-shells.elf'})
        manifest=Path(temp)/'install.json';manifest.write_text(json.dumps(plan))
        disk=Path(temp)/'root.img';data=build(None,ROOT/'build',ROOT/'rootfs',manifest,spare_inodes=64,spare_blocks=2048)
        write_image(disk,data)
        if hasattr(data,'close'):data.close()
        guest=Guest(qemu,iso,disk,ram,2)
        try:
            for round in range(4):
                for line in range(2):guest.expect(f'ROUND_{round}_LINE_{line}\r\nroot@[A-Za-z0-9.-]+# ',line)
                # A partial line on COM1 cannot block input or execution on COM2.
                guest.send('echo PAR',0)
                guest.command('echo OTHER_LINE',1,'OTHER_LINE\n')
                guest.send('TIAL\n',0);guest.expect(r'echo PARTIAL\r\nPARTIAL\r\nroot@[A-Za-z0-9.-]+# ')
                for line in range(2):
                    guest.command(f'echo ALIAS_{line} > /dev/tty',line,f'ALIAS_{line}\n')
                    guest.command(f'echo FILE_{line} > /tmp/line{line}',line)
                    guest.command(f'cat /tmp/line{line}',line,f'FILE_{line}\n')
                # Interrupt a foreground cat on COM1 while COM2 continues to run.
                guest.send('cat -u\n',0);guest.expect('cat -u\r\n')
                guest.send('CAT_READY\n',0);guest.expect('CAT_READY\r\nCAT_READY\r\n')
                guest.command('echo OTHER_ALIVE',1,'OTHER_ALIVE\n')
                guest.send('\x03',0);guest.expect('\r\nroot@[A-Za-z0-9.-]+# ')
                for line in range(2):
                    guest.send('\n'*8,line)
                    for _ in range(8):guest.expect('# ',line)
                    guest.command(f'rm /tmp/line{line}',line)
                guest.send('\x04',0)
                guest.command('echo SURVIVING_SHELL',1,'SURVIVING_SHELL\n')
                guest.send('\x04',1)
            guest.expect('SHELL_RECOVERY_PASSED\r\n')
            for line in range(2):
                if b'PANIC' in guest.buffers[line]:raise AssertionError('kernel panic')
        finally:
            for n,out in enumerate(guest.buffers):(ROOT/'build'/f'terminal-shells-{ram}.tty{n}.log').write_bytes(out)
            guest.diagnostics.seek(0);(ROOT/'build'/f'terminal-shells-{ram}.qemu.log').write_bytes(guest.diagnostics.read())
            guest.close()
    print(f'PASS: {ram} concurrent V7 shells, partial input, ctty writes, foreground interrupts, empty lines, EOF, four restarts and resource recovery')


if __name__=='__main__':main()
