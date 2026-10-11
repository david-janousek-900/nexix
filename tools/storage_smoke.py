"""Cold-boot a disposable IDE root three times and verify durable V7 changes."""
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import tempfile
import time

ram = sys.argv[3] if len(sys.argv) > 3 else '128M'
log = bytearray()

def boot(disk, commands, initial=rb'(?s)NEXIX_ROOT_IDE\r\nNEXIX_ROOT_MOUNTED.*NEXIX_BOOT_OK\r\n# '):
    command = [sys.argv[1], '-machine', 'pc', '-accel', 'tcg', '-cpu', 'qemu64',
               '-smp', '1', '-m', ram, '-boot', 'd', '-cdrom', sys.argv[2],
               '-drive', f'file={disk},format=raw,if=ide,index=0',
               '-display', 'none', '-serial', 'stdio', '-monitor', 'none', '-no-reboot']
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
    output = bytearray()
    def exchange(line, pattern):
        start = len(output)
        if line is not None:
            process.stdin.write(line.encode() + b'\n')
            process.stdin.flush()
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            if re.search(pattern, bytes(output[start:])):
                return
            if b'PANIC' in output and b'PANIC' not in pattern:
                raise RuntimeError('kernel panic')
            if process.poll() is not None:
                raise RuntimeError('QEMU exited')
            if select.select([process.stdout], [], [], .2)[0]:
                chunk = os.read(process.stdout.fileno(), 4096)
                if not chunk:
                    raise RuntimeError('closed COM1')
                output.extend(chunk)
        raise RuntimeError(f'timeout: {line}')
    try:
        exchange(None, initial)
        for line, pattern in commands:
            exchange(line, pattern)
    finally:
        process.terminate()
        try:
            remaining, _ = process.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            remaining, _ = process.communicate()
        output.extend(remaining)
        log.extend(output)

def response(text):
    return rb'\r\n' + re.escape(text.encode()) + rb'\r\n# '

error = None
try:
    with tempfile.TemporaryDirectory(prefix='nexix-ide-') as directory:
        disk = Path(directory) / 'root.img'
        seed = Path('build/root.img').read_bytes()
        disk.write_bytes(seed)
        boot(disk, [
            ('echo persisted > /tmp/data; ln /tmp/data /tmp/link; cp /bin/echo /tmp/exec; echo STORED', response('STORED')),
            # >10 sectors exercises the single-indirect map on the real device.
            ('cp /bin/sh /tmp/large; echo LARGE', response('LARGE')),
        ])
        boot(disk, [
            ('cat /tmp/data; cat /tmp/link; /tmp/exec DISK_EXEC', rb'\r\npersisted\r\npersisted\r\nDISK_EXEC\r\n# '),
            ('/tmp/large -c "echo INDIRECT_EXEC"', response('INDIRECT_EXEC')),
            ('echo updated >> /tmp/link; rm /tmp/data /tmp/exec /tmp/large; echo CHANGED', response('CHANGED')),
        ])
        boot(disk, [
            ('cat /tmp/link; ls /tmp', rb'\r\npersisted\r\nupdated\r\nlink\r\n# '),
            ('rm /tmp/link; cp /bin/sh /tmp/reused; /tmp/reused -c "echo REUSED"; rm /tmp/reused; ls /tmp; echo CLEAN', rb'\r\nREUSED\r\nCLEAN\r\n# '),
        ])
        # Independent on-disk free-block and free-inode totals must be restored.
        if disk.read_bytes()[512+418:512+424] != seed[512+418:512+424]:
            raise RuntimeError('free blocks/inodes leaked across cold boots')
        disk.write_bytes(bytes(len(seed)))
        boot(disk, [], rb'PANIC: cannot mount IDE root')
except (RuntimeError, OSError) as problem:
    error = str(problem)
finally:
    target = Path('build') / f'storage-{ram}.log'
    target.write_bytes(log)
if error:
    sys.stdout.buffer.write(log)
    sys.exit(f'FAIL: {error}; see {target}')
print(f'PASS: {ram} IDE root, three cold boots, persistent data/links/ELF, indirect blocks/reuse, invalid-root rejection')
