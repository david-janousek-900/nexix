"""Hardware FP replacement acceptance on a private full root."""
import sys
from private_guest import private_guest

qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'fpsim',{'/tmp/fpsim':'fpsim-equivalent.elf'}) as guest:
    guest.expect(r'(?s)NEXIX_BOOT_OK\r\n# ')
    guest.command('/tmp/fpsim',stdout='FPSIM_EQUIVALENT_OK\n')
print(f'PASS: {ram} native libfpsim/fakfp equivalent arithmetic, conversion and SIGILL')
