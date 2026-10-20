from pathlib import Path
import sys
from private_guest import private_guest
qemu,iso,ram=sys.argv[1:4]
# Exceeds the 64 KiB in-core temp cache and several line-reference growth pages.
Path('build/vi-storage-input').write_text(''.join(f'{n:06d} '+('abcdefghij'*8)+'\n' for n in range(6000)))
with private_guest(qemu,iso,ram,'vi-storage',{'/tmp/vistorage':'vi-storage.elf','/tmp/input':'vi-storage-input'}) as guest:
 guest.timeout=120
 guest.expect(r'root@nexix.localdomain# ')
 guest.command('/tmp/vistorage',stdout='VI_STORAGE_OK\n')
print(f'PASS: {ram} native vi buffer growth, temp ownership/collision, full disk and cleanup')
