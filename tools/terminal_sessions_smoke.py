"""Check V7 controlling-terminal inheritance and two physical line disciplines."""
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
    with tempfile.TemporaryDirectory(prefix='nx-sessions-') as temp:
        plan=json.loads(MANIFEST.read_text())
        entry=next(e for e in plan['entries'] if e['path']=='/sbin/init')
        entry.update(component='distribution.config',format='data',source={'root':'programs','path':'terminal-sessions.elf'})
        manifest=Path(temp)/'install.json';manifest.write_text(json.dumps(plan))
        disk=Path(temp)/'root.img';data=build(None,ROOT/'build',ROOT/'rootfs',manifest,spare_inodes=64,spare_blocks=2048)
        write_image(disk,data)
        if hasattr(data,'close'):data.close()
        guest=Guest(qemu,iso,disk,ram,2)
        def expect(label,line):guest.expect(f'{label}_{line}\r\n',line)
        try:
            for line in range(2):
                expect('CTTY',line);expect('CTTY',line);expect('INHERITED',line);expect('COOKED',line)
            for line in range(2):guest.send('ab#c\n',line);expect('EMPTY',line)
            for line in range(2):guest.send('\n',line);expect('EOF',line)
            for line in range(2):guest.send('\x04',line);expect('SIGNAL',line)
            guest.send('\x03',0);expect('FLOW',0)
            guest.pump(.2)
            if b'FLOW_1' in guest.buffers[1]:raise AssertionError('cross-terminal SIGINT')
            guest.send('\x03',1);expect('FLOW',1)
            guest.send('\x13go\n',0)
            guest.send('go\n',1);expect('HELD',1);expect('DONE',1)
            guest.pump(.2)
            if b'HELD_0' in guest.buffers[0]:raise AssertionError('XOFF ignored')
            guest.send('\x11',0);expect('HELD',0);expect('DONE',0)
            guest.expect('SESSIONS_PASSED\r\n')
        finally:
            for n,out in enumerate(guest.buffers):(ROOT/'build'/f'terminal-sessions-{ram}.tty{n}.log').write_bytes(out)
            guest.diagnostics.seek(0);(ROOT/'build'/f'terminal-sessions-{ram}.qemu.log').write_bytes(guest.diagnostics.read())
            guest.close()
    print(f'PASS: {ram} distinct controlling terminals/groups, fork/exec inheritance, cooked editing, empty lines, EOF, SIGINT and XOFF isolation')


if __name__=='__main__':main()
