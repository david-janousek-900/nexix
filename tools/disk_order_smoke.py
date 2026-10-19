"""Cold-boot a disposable primary-slave ATA disk through V7 HP interleaved queues."""
import os
import hashlib
from pathlib import Path
import select
import struct
import subprocess
import sys
import tempfile
import time
from mk_v7fs import Image, REG


def contents(image, number):
    record = image[1024+(number-1)*64:1024+number*64]
    size = (int.from_bytes(record[8:10], 'little') << 16) | int.from_bytes(record[10:12], 'little')
    assert size <= 10*512
    result = bytearray()
    for offset in range(12, 42, 3):
        block = (record[offset] << 16) | record[offset+1] | record[offset+2] << 8
        result.extend(image[block*512:block*512+512] if block else bytes(512))
    return bytes(result[:size])


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024*1024), b''):
            result.update(block)
    return result.digest()


def check_media(disks):
    image = bytearray()
    with disks[0].open('rb') as first, disks[1].open('rb') as second:
        sources = (first, second)
        for logical in range(1024):
            source = sources[logical % 2]
            source.seek((9614+logical//2)*512)
            image.extend(source.read(512))
        for unit, source in enumerate(sources):
            source.seek(0)
            header = source.read(512)
            assert header[:len(b'ORDER TEST ONLY73')] == b'ORDER TEST ONLY73' and header[32] == ord('0')+unit
    entries = contents(image, 2)
    files = {entries[i+2:i+16].rstrip(b'\0'): struct.unpack_from('<H', entries, i)[0]
             for i in range(0, len(entries), 16)}
    assert contents(image, files[b'persist']) == bytes(i & 255 for i in range(700))
    assert contents(image, files[b'hello']) == b'ordering-volume\n'


def boot(qemu, iso, ram, disks, number, owned=True):
    command = [qemu, '-machine', 'pc', '-accel', 'tcg', '-cpu', 'qemu64', '-smp', '1',
               '-m', ram, '-drive', f'file={iso},format=raw,media=cdrom,if=ide,index=3,readonly=on',
               '-boot', 'd', '-display', 'none', '-serial', 'stdio',
               '-monitor', 'none', '-no-reboot', '-drive', f'file={disks[0]},format=raw,if=ide,index=1',
               '-drive', f'file={disks[1]},format=raw,if=ide,index=2']
    process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = bytearray()
    try:
        deadline = time.monotonic()+45
        while process.poll() is None and time.monotonic() < deadline:
            ready, _, _ = select.select([process.stdout], [], [], .2)
            if ready:
                chunk = os.read(process.stdout.fileno(), 4096)
                if not chunk:
                    break
                output.extend(chunk)
            if b'BOOT OK' in output or b'PANIC' in output:
                break
    finally:
        process.terminate()
        try:
            extra, _ = process.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            extra, _ = process.communicate()
        output.extend(extra)
    log = Path(f'build/disk-order-{ram}-boot{number}.log')
    log.write_bytes(output)
    required = [b'NEXIX_DISK_ORDER_OK', b'ROOT EXEC CLEANUP OK', b'BOOT OK']
    if owned:
        required.append(b'NEXIX_ORDER_IDE_OK')
    if number == 2:
        required.append(b'NEXIX_ORDER_PERSIST_OK')
    if not all(marker in output for marker in required) or b'PANIC' in output:
        raise SystemExit(f'FAIL: HP interleave boot {number} ({ram}); see {log}')
    if owned:
        check_media(disks)
    else:
        assert b'NEXIX_ORDER_IDE_OK' not in output


def main():
    with tempfile.TemporaryDirectory(prefix='nexix-order-') as directory:
        disks = [Path(directory)/f'auxiliary{unit}.img' for unit in range(2)]
        volume = Image(1024)
        volume.inode(5, REG|0o644, b'ordering-volume\n')
        volume.directory(2, 2, [('hello', 5)])
        image = volume.finish()
        for unit, disk in enumerate(disks):
            with disk.open('wb') as target:
                target.truncate(340670*512)
                header = bytearray(512)
                header[:len(b'ORDER TEST ONLY73')] = b'ORDER TEST ONLY73'
                header[32] = ord('0')+unit
                target.write(header)
                for block in (418, 836):
                    target.seek(block*512)
                    target.write(bytes((block+i)&255 for i in range(512)))
                for logical in range(unit, 1024, 2):
                    target.seek((9614+logical//2)*512)
                    target.write(image[logical*512:(logical+1)*512])
        for number in (1, 2):
            boot(sys.argv[1], sys.argv[2], sys.argv[3], disks, number)
        with disks[0].open('r+b') as target:
            target.write(bytes(len(b'ORDER TEST ONLY73')))
        before = [digest(disk) for disk in disks]
        boot(sys.argv[1], sys.argv[2], sys.argv[3], disks, 3, owned=False)
        assert [digest(disk) for disk in disks] == before
    print(f'PASS: two-drive HP stripe mapping, real queued reads, mounted aliases and persistent V7 filesystem across two cold boots; unmarked media unchanged ({sys.argv[3]})')


if __name__ == '__main__':
    main()
