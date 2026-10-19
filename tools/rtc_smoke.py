"""Verify the boot clock with a fixed UTC RTC and normal shell startup."""
import re
import subprocess
import sys
from pathlib import Path

command = [sys.argv[1], '-machine', 'pc', '-accel', 'tcg', '-cpu', 'qemu64',
           '-smp', '1', '-m', '128M', '-rtc', 'base=2024-02-29T12:34:56,clock=vm',
           '-cdrom', sys.argv[2], '-display', 'none', '-serial', 'stdio',
           '-monitor', 'none', '-no-reboot']
p = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
try:
    output, _ = p.communicate(timeout=12)
    early = True
except subprocess.TimeoutExpired:
    early = False
    p.terminate()
    output, _ = p.communicate(timeout=3)
Path('build/rtc-boot.log').write_bytes(output)
match = re.search(rb'RTC UTC epoch=(?:0x)?([0-9a-fA-F]+)', output)
if (early or not match or not 1709210096 <= int(match[1], 16) <= 1709210106
        or b'RTC OK' not in output or b'PANIC' in output or b'# ' not in output):
    sys.stdout.buffer.write(output)
    sys.exit('FAIL: fixed RTC boot clock or shell; see build/rtc-boot.log')
print('PASS: QEMU RTC 2024-02-29 12:34:56 UTC initializes kernel epoch and boots shell')
