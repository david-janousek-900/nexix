"""Check installed aliases, guest privilege metadata and pinned documentation."""
from pathlib import Path
from install_manifest import read_stage
entries,payloads=read_stage(Path('build/stage'))
records={e['path']:e for e in entries}
for path in ['/bin/vi','/bin/ex','/bin/view','/usr/libexec/expreserve','/usr/libexec/exrecover']:
 e=records[path]
 assert e['component']=='cmd.vi' and e['format']=='elf' and e['uid']==e['gid']==0
 assert e['mode']==('4755' if path.endswith('/expreserve') else '0755')
 assert payloads[path].startswith(b'\x7fELF\x02\x01\x01')
assert payloads['/bin/vi']==payloads['/bin/ex']==payloads['/bin/view']
for path,source in [('/usr/man/man1/vi.1','vi.1'),('/usr/man/man1/view.1','vi.1'),('/usr/man/man1/ex.1','ex.1'),('/usr/doc/vi/LICENSE','LICENSE')]:
 assert payloads[path]==Path('reference/ex-vi/050325',source).read_bytes()
assert payloads['/usr/doc/vi/NOTICE']==Path('THIRD_PARTY_NOTICES.md').read_bytes()
assert payloads['/usr/doc/vi/PORT.md']==Path('docs/vi-port.md').read_bytes()
assert records['/var/preserve']['mode']=='0755' and records['/var/preserve']['uid']==0
assert records['/usr/libexec/exrecover']['mode']=='0755'
print('PASS: installed vi aliases, checked helper permissions, pinned manuals and full notices')
