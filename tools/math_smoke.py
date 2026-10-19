"""Complete math interface acceptance on a private full root."""
import sys
from private_guest import private_guest

qemu,iso,ram=sys.argv[1:4]
with private_guest(qemu,iso,ram,'math',{'/tmp/math':'math-coverage.elf'}) as guest:
    guest.expect(r'(?s)BOOT OK\r\nroot@[A-Za-z0-9.-]+# ')
    guest.command('/tmp/math',stdout='MATH_COVERAGE_OK\n')
print(f'PASS: {ram} gamma/signgam, large arguments, exceptional math and Bessel order boundaries')
