"""Exercise the physical COM2 transport through a second QEMU pipe endpoint."""
import json
from pathlib import Path
import re
import sys
import tempfile
from guest_acceptance import Guest
from install_manifest import MANIFEST, ROOT
from mk_rootfs import build
from mk_v7fs import write_image


def main():
    qemu,iso,ram=sys.argv[1:4]
    packaged='--packaged' in sys.argv[4:]
    log=ROOT/'build'/f'serial-secondary-{ram}'
    with tempfile.TemporaryDirectory(prefix='nx-com2-') as temp:
        manifest=json.loads(MANIFEST.read_text())
        manifest['entries'].append({'path':'/tmp/uartcheck','type':'file','mode':'0755',
            'uid':0,'gid':0,'component':'distribution.config','format':'data',
            'source':{'root':'programs','path':'serial-secondary-guest.elf'}})
        plan=Path(temp)/'install.json';plan.write_text(json.dumps(manifest))
        disk=Path(temp)/'root.img'
        data=build(None,ROOT/'build',ROOT/'rootfs',plan,spare_inodes=64,spare_blocks=2048,fixture_overrides={'/etc/ttys':b''})
        write_image(disk,data)
        if hasattr(data,'close'):data.close()
        guest=Guest(qemu,iso,disk,ram,2)
        try:
            guest.expect(r'(?s)NEXIX_BOOT_OK\r\n# ')
            guest.send('/tmp/uartcheck'+(' packaged' if packaged else '')+'; echo COM2_STATUS_$?\n')
            guest.expect(r'COM1_READY\r\n')
            guest.expect(b'COM2_READY\n',1)
            guest.send(bytes((i*37)&255 for i in range(512)),1)
            expected=bytes((i*19)&255 for i in range(512))*16
            before,_=guest.expect(re.escape(expected),1)
            if before:raise AssertionError('unexpected COM2 bytes')
            guest.expect(r'COM1_DONE\r\nCOM2_STATUS_0\r\n# ')
            guest.command('echo COM1_SURVIVES',stdout='COM1_SURVIVES\n')
            if bytes(guest.buffers[1])!=b'COM2_READY\n'+expected:
                raise AssertionError('COM2 stream contamination')
        finally:
            for n,output in enumerate(guest.buffers):log.with_suffix(f'.tty{n}.log').write_bytes(output)
            guest.diagnostics.seek(0);log.with_suffix('.qemu.log').write_bytes(guest.diagnostics.read())
            guest.close()
    print(f'PASS: {ram} COM2 IRQ3, binary RX/TX, queue drain and independent COM1 output'+('; packaged ttyS1 metadata' if packaged else ''))


if __name__=='__main__':main()
