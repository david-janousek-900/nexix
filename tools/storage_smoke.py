"""Cold-boot a disposable IDE root three times and verify durable V7 changes."""
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import tempfile
from mk_rootfs import build
import time

ram = sys.argv[3] if len(sys.argv) > 3 else '128M'
log = bytearray()

def boot(disk, commands, initial=rb'(?s)ROOT IDE\r\n(?:\[[^\]\r\n]+\] )?ROOT MOUNTED.*BOOT OK\r\nroot@[A-Za-z0-9.-]+# ', rtc='2024-02-29T12:34:56'):
    command = [sys.argv[1], '-machine', 'pc', '-accel', 'tcg', '-cpu', 'qemu64,+rdrand',
               '-smp', '1', '-m', ram, '-rtc', f'base={rtc},clock=vm',
               '-boot', 'd', '-cdrom', sys.argv[2],
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
    return rb'\r\n' + re.escape(text.encode()) + rb'\r\nroot@[A-Za-z0-9.-]+# '

def inode_times(disk, name):
    # Decode disk bytes independently of the kernel and its inode cache.
    data = disk.read_bytes()
    def u32(raw):
        return (int.from_bytes(raw[:2], 'little') << 16) | int.from_bytes(raw[2:], 'little')
    def inode(number):
        start = 1024 + (number - 1) * 64
        return data[start:start + 64]
    directory = inode(6)  # mk_rootfs.py assigns /tmp inode 6.
    size = u32(directory[8:12])
    if size > 512:
        raise RuntimeError('timestamp fixture directory exceeds one direct block')
    address = directory[12:15]
    block = (address[0] << 16) | (address[2] << 8) | address[1]
    for offset in range(block * 512, block * 512 + size, 16):
        entry = data[offset:offset + 16]
        if entry[2:].rstrip(b'\0') == name.encode():
            node = inode(int.from_bytes(entry[:2], 'little'))
            return tuple(u32(node[offset:offset+4]) for offset in (52, 56, 60))
    raise RuntimeError(f'missing timestamp fixture: {name}')

error = None
try:
    with tempfile.TemporaryDirectory(prefix='nexix-ide-') as directory:
        disk = Path(directory) / 'root.img'
        seed = bytes(build(None,None,None,stage=Path('build/stage'),spare_inodes=2048,spare_blocks=32768,fixture_overrides={'/etc/ttys':b''}))
        disk.write_bytes(seed)
        boot(disk, [
            ('echo persisted > /tmp/data; touch /tmp/data; ln /tmp/data /tmp/link; cp /bin/echo /tmp/exec; echo STORED', response('STORED')),
            # >10 sectors exercises the single-indirect map on the real device.
            ('cp /bin/sh /tmp/large; echo LARGE', response('LARGE')),
        ])
        created = inode_times(disk, 'data')
        if not all(1709210096 <= value <= 1709210126 for value in created):
            raise RuntimeError(f'creation timestamps do not match RTC: {created}')
        if inode_times(disk, 'link') != created:
            raise RuntimeError('hard-link timestamps differ')
        boot(disk, [
            ('cat /tmp/data; cat /tmp/link; /tmp/exec DISK_EXEC', rb'\r\npersisted\r\npersisted\r\nDISK_EXEC\r\nroot@[A-Za-z0-9.-]+# '),
            ('/tmp/large -c "echo INDIRECT_EXEC"', response('INDIRECT_EXEC')),
            ('echo updated >> /tmp/link; touch /tmp/link; rm /tmp/data /tmp/exec /tmp/large; echo CHANGED', response('CHANGED')),
        ], rtc='2024-03-01T12:34:56')
        updated = inode_times(disk, 'link')
        if not all(1709296496 <= value <= 1709296526 for value in updated):
            raise RuntimeError(f'read/write/link-change timestamps do not match new RTC: {updated}')
        if not all(new > old for new, old in zip(updated, created)):
            raise RuntimeError('timestamps did not advance across cold boots')
        boot(disk, [
            ('ls -l /tmp/link', rb'Mar  1.*link\r\nroot@[A-Za-z0-9.-]+# '),
            ('cat /tmp/link; ls /tmp', rb'\r\npersisted\r\nupdated\r\nlink\r\nroot@[A-Za-z0-9.-]+# '),
            ('rm /tmp/link; cp /bin/sh /tmp/reused; /tmp/reused -c "echo REUSED"; rm /tmp/reused; ls /tmp; echo CLEAN', rb'\r\nREUSED\r\nCLEAN\r\nroot@[A-Za-z0-9.-]+# '),
        ], rtc='2024-03-02T12:34:56')
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
print(f'PASS: {ram} IDE root, three cold boots, persistent data/links/ELF and RTC timestamps, indirect blocks/reuse, invalid-root rejection')
