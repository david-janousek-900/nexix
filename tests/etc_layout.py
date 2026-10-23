"""BSD directory layout (task 434): /etc holds configuration and rc scripts
only, in both installation manifests and in the built distribution image;
programs live in the binary directories."""
import json
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from install_manifest import load_retired
from mk_v7fs import REG
from upgrade_disk import Volume

BINARY_DIRECTORIES = ('/bin/', '/sbin/', '/usr/bin/', '/usr/sbin/', '/usr/libexec/', '/usr/lib/', '/lib/')
retired = {r['path'] for r in load_retired()}
assert len(retired) == 9
for name in ('docs/native-install.json', 'docs/bootstrap-install.json'):
    entries = json.loads((ROOT / name).read_text())['entries']
    paths = {e['path'] for e in entries}
    assert {'/sbin'} <= paths, name
    for e in entries:
        p = e['path']
        if p.startswith('/etc/'):
            # Configuration: plain data, never executable, never a program.
            assert e['type'] == 'file' and e.get('format') in ('data', 'empty'), (name, p)
            assert not int(e['mode'], 8) & 0o111 and e['component'] == 'distribution.config', (name, p)
        if e.get('format') == 'elf':
            assert p.startswith(BINARY_DIRECTORIES), (name, p)
        assert p not in retired, (name, p)
    if name == 'docs/native-install.json':
        assert {'/usr/bin', '/usr/sbin', '/usr/libexec'} <= paths
image = ROOT / 'build/distribution.img'
volume = Volume(image.read_bytes())
programs = [p for p, n in volume.paths.items() if p.startswith('/etc/') and n.kind == REG and volume.contents(n)[:4] == b'\x7fELF']
assert not programs, programs
etc = sorted(p for p in volume.paths if p.startswith('/etc/'))
print(f'PASS: /etc in both manifests and the distribution image holds only configuration ({len(etc)} files); '
      f'ELF programs only in {", ".join(d.rstrip("/") for d in BINARY_DIRECTORIES)}; {len(retired)} retired paths absent')
