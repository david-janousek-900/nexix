"""Exercise the normal PID 1 boot and single-user shell through COM1."""
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import time

ram = sys.argv[3] if len(sys.argv) > 3 else '128M'
command = [sys.argv[1], '-machine', 'pc', '-accel', 'tcg', '-cpu', 'qemu64',
           '-smp', '1', '-m', ram, '-cdrom', sys.argv[2], '-display', 'none',
           '-serial', 'stdio', '-monitor', 'none', '-no-reboot']
output = bytearray()
error = None
try:
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
except FileNotFoundError:
    sys.exit('FAIL: QEMU is not installed')

def exchange(line, expected, raw=False):
    start = len(output)
    if line is not None:
        process.stdin.write(line if raw else line + b'\n')
        process.stdin.flush()
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        current = bytes(output[start:])
        if b'PANIC' in output:
            raise RuntimeError('kernel panic')
        if expected.search(current):
            return current
        if process.poll() is not None:
            raise RuntimeError('QEMU exited unexpectedly')
        ready, _, _ = select.select([process.stdout], [], [], 0.2)
        if ready:
            data = os.read(process.stdout.fileno(), 4096)
            if not data:
                raise RuntimeError('COM1 output closed')
            output.extend(data)
    raise RuntimeError('timeout waiting for shell response')

try:
    exchange(None, re.compile(rb'NEXIX_INIT_PID1\r\nNEXIX_BOOT_OK\r\n# '))
    reply = exchange(b'echo NEXIX_INIT_CHILD_$$; pwd; ls /bin',
                     re.compile(rb'\r\nNEXIX_INIT_CHILD_2\r\n/\r\ncat\r\ncp\r\ndate\r\necho\r\nln\r\nls\r\npwd\r\nrm\r\nsh\r\ntouch\r\n# '))
    # A copied executable has no embedded-catalog entry: it must be read from disk.
    exchange(b'echo disk > /tmp/boot-file; cat /tmp/boot-file; cp /bin/echo /tmp/disk-echo; /tmp/disk-echo NEXIX_DISK_EXEC; rm /tmp/boot-file /tmp/disk-echo; echo NEXIX_BOOT_WRITE_OK',
             re.compile(rb'\r\ndisk\r\nNEXIX_DISK_EXEC\r\nNEXIX_BOOT_WRITE_OK\r\n# '))
    exchange(b'\x04', re.compile(rb'Nexix 0\.1\.0\r\n# '), raw=True)
    reply = exchange(b'echo NEXIX_RESTART_CHILD_$$',
                     re.compile(rb'\r\nNEXIX_RESTART_CHILD_([0-9]+)\r\n# '))
    pid = int(re.search(rb'\r\nNEXIX_RESTART_CHILD_([0-9]+)\r\n', reply)[1])
    if pid <= 2:
        raise RuntimeError('init did not create a replacement shell')
except (RuntimeError, BrokenPipeError) as problem:
    error = str(problem)
finally:
    process.terminate()
    try:
        remaining, _ = process.communicate(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        remaining, _ = process.communicate()
    output.extend(remaining)
log = Path('build') / f'boot-{ram}.log'
log.write_bytes(output)
sys.stdout.buffer.write(output)
if error:
    sys.exit(f'FAIL: {error}; see {log}')
if b'PANIC' in output or b'NEXIX_ROOT_MOUNTED\r\n' not in output:
    sys.exit(f'FAIL: root/PID 1 boot; see {log}')
print(f'PASS: {ram} writable root, filesystem exec, PID 1 and shell restart')
