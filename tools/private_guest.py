"""Disposable full roots with explicitly private acceptance ELF fixtures."""
from contextlib import contextmanager
import json
from pathlib import Path
import tempfile
from guest_acceptance import Guest
from install_manifest import MANIFEST,ROOT,guest_path
from mk_rootfs import build
from mk_v7fs import write_image


@contextmanager
def private_guest(qemu,iso,ram,label,artifacts,terminals=1,single_user=True):
    with tempfile.TemporaryDirectory(prefix='nx-private-') as temp:
        plan=json.loads(MANIFEST.read_text())
        for path,artifact in artifacts.items():
            guest_path(path)
            record={'path':path,'type':'file','mode':'0755','uid':0,'gid':0,
                'component':'distribution.config','format':'data',
                'source':{'root':'programs','path':artifact}}
            existing=next((e for e in plan['entries'] if e['path']==path),None)
            if existing is None:plan['entries'].append(record)
            else:existing.clear();existing.update(record)
        manifest=Path(temp)/'install.json';manifest.write_text(json.dumps(plan))
        disk=Path(temp)/'root.img';data=build(None,ROOT/'build',ROOT/'rootfs',manifest,spare_inodes=256,spare_blocks=8192,fixture_overrides={'/etc/ttys':b''} if single_user else None)
        write_image(disk,data)
        if hasattr(data,'close'):data.close()
        guest=Guest(qemu,iso,disk,ram,terminals)
        try:yield guest
        finally:
            for n,out in enumerate(guest.buffers):(ROOT/'build'/f'{label}-{ram}.tty{n}.log').write_bytes(out)
            guest.diagnostics.seek(0);(ROOT/'build'/f'{label}-{ram}.qemu.log').write_bytes(guest.diagnostics.read())
            guest.close()
