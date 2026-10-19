"""Cold-boot a disposable primary-slave ATA disk through V7 controller views."""
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
    block = (record[12] << 16) | record[13] | record[14] << 8
    assert size <= 512
    return image[block*512:block*512+size]


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024*1024), b''):
            result.update(block)
    return result.digest()


def check_media(path):
    with path.open('rb') as source:
        source.seek(5000*512)
        image = source.read(2000*512)
        source.seek(512)
        assert source.read(8) == b'adjacent'
    entries = contents(image, 2)
    files = {entries[i+2:i+16].rstrip(b'\0'): struct.unpack_from('<H', entries, i)[0]
             for i in range(0, len(entries), 16)}
    assert contents(image, files[b'persist']) == b'controller-data'
    assert contents(image, files[b'hello']) == b'controller-volume\n'


def boot(qemu, iso, ram, disk, number, owned=True):
    command = [qemu, '-machine', 'pc', '-accel', 'tcg', '-cpu', 'qemu64', '-smp', '1',
               '-m', ram, '-cdrom', iso, '-display', 'none', '-serial', 'stdio',
               '-monitor', 'none', '-no-reboot', '-drive', f'file={disk},format=raw,if=ide,index=1']
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
    log = Path(f'build/disk-{ram}-boot{number}.log')
    log.write_bytes(output)
    required = [b'NEXIX_DISK_OK', b'ROOT EXEC CLEANUP OK', b'BOOT OK']
    if owned:
        required.append(b'NEXIX_DISK_IDE_OK')
    if number == 2:
        required.append(b'NEXIX_DISK_PERSIST_OK')
    if not all(marker in output for marker in required) or b'PANIC' in output:
        raise SystemExit(f'FAIL: controller ATA boot {number} ({ram}); see {log}')
    if owned:
        check_media(disk)
    else:
        assert b'NEXIX_DISK_IDE_OK' not in output


def main():
    with tempfile.TemporaryDirectory(prefix='nexix-disk-') as directory:
        disk = Path(directory)/'auxiliary.img'
        volume = Image(2000)
        volume.inode(5, REG|0o644, b'controller-volume\n')
        volume.directory(2, 2, [('hello', 5)])
        with disk.open('wb') as target:
            target.truncate(340670*512)
            target.write(b'NEXIX_DISK_TEST_ONLY70')
            target.seek(5000*512)
            target.write(volume.finish())
        for number in (1, 2):
            boot(sys.argv[1], sys.argv[2], sys.argv[3], disk, number)
        with disk.open('r+b') as target:
            target.write(bytes(22))
        before = digest(disk)
        boot(sys.argv[1], sys.argv[2], sys.argv[3], disk, 3, owned=False)
        assert digest(disk) == before
    print(f'PASS: five disk views, auxiliary ATA mount/alias guards, adjacent partition, two cold boots and independent persisted bytes; unmarked disk unchanged ({sys.argv[3]})')


if __name__ == '__main__':
    main()
