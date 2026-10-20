from pathlib import Path
import sys
from private_guest import private_guest
from guest_acceptance import Guest
qemu,iso,ram=sys.argv[1:4]
Path('build/vi-corrupt').write_bytes(bytes(4096))
artifacts={'/tmp/vipreserve':'vi-preserve.elf','/tmp/ex':'vi-dev.elf','/usr/libexec/expreserve':'vi-expreserve.elf','/usr/libexec/exrecover':'vi-exrecover.elf','/tmp/corrupt':'vi-corrupt'}
with private_guest(qemu,iso,ram,'vi-preserve',artifacts) as guest:
 guest.expect(r'root@nexix.localdomain# ')
 guest.command('chmod 4755 /usr/libexec/expreserve')
 guest.command('/usr/libexec/expreserve < /tmp/corrupt 2>/tmp/error',status=1)
 guest.command('/tmp/vipreserve save',stdout='VI_PRESERVED\n')
 disk=guest.disk;guest.process.terminate();guest.process.wait(timeout=3)
 reboot=Guest(qemu,iso,disk,ram)
 try:
  reboot.expect(r'root@nexix.localdomain# ')
  reboot.command('/tmp/vipreserve list',stdout='VI_RECOVERY_LISTED\n')
  reboot.command('/tmp/vipreserve deny',stdout='VI_RECOVERY_DENIED\n')
  reboot.command('/tmp/vipreserve recover',stdout='VI_RECOVERED\n')
  reboot.command('cp /tmp/corrupt /var/preserve/Exbad')
  reboot.command('/usr/libexec/exrecover /var/preserve invalid 2>/tmp/error',status=1)
 finally:
  Path(f'build/vi-recovery-{ram}.tty0.log').write_bytes(reboot.buffers[0]);reboot.close()
print(f'PASS: {ram} SIGHUP/private preservation, wrong-user rejection, cold-boot recovery and malformed input')
