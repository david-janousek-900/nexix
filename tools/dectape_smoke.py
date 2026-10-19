"""Cold-boot a disposable primary-slave ATA disk through random-block TC media."""
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
    roots = [(record[i] << 16) | record[i+1] | record[i+2] << 8 for i in range(12, 51, 3)]
    def pointer(block, slot):
        assert 0 < block < len(image)//512
        at = block*512+slot*4
        return (int.from_bytes(image[at:at+2], 'little') << 16) | int.from_bytes(image[at+2:at+4], 'little')
    result = bytearray()
    for logical in range((size+511)//512):
        if logical < 10:
            block = roots[logical]
        else:
            logical -= 10
            depth, span = 1, 128
            while logical >= span:
                logical -= span
                depth += 1
                span *= 128
            assert depth <= 3
            block = roots[9+depth]
            for _ in range(depth):
                span //= 128
                index, logical = divmod(logical, span)
                block = pointer(block, index) if block else 0
        assert block < len(image)//512
        result.extend(image[block*512:block*512+512] if block else bytes(512))
    return bytes(result[:size])


def files(image):
    entries = contents(image, 2)
    return {entries[i+2:i+16].rstrip(b'\0'): struct.unpack_from('<H', entries, i)[0]
            for i in range(0, len(entries), 16)}


def tp_image():
    # V7 tp.h's PDP-11 tent is exactly 64 bytes, independent of host C layouts.
    image = bytearray(578*512)
    entry = bytearray(64)
    entry[:6] = b'hello\0'
    struct.pack_into('<H', entry, 32, 0o644)
    entry[34:36] = bytes((12, 34))
    struct.pack_into('<H', entry, 38, 15)
    struct.pack_into('<HH', entry, 40, 0x1234, 0x5678)
    struct.pack_into('<H', entry, 44, 25)
    struct.pack_into('<H', entry, 62, (-sum(struct.unpack('<32H', entry))) & 65535)
    image[512:576] = entry
    image[25*512:25*512+15] = b'dectape-format\n'
    return image


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024*1024), b''):
            result.update(block)
    return result.digest()


def check_media(path):
    with path.open('rb') as source:
        source.seek(5000*512)
        outer = source.read(2000*512)
    names = files(outer)
    tape = contents(outer, names[b'tp'])
    expected = tp_image()
    expected[577*512:577*512+7] = b'tc-last'
    assert tape == expected
    entry = tape[512:576]
    assert sum(struct.unpack('<32H', entry)) & 65535 == 0
    inner = contents(outer, names[b'fs'])
    assert len(inner) == 578*512
    inner_names = files(inner)
    assert contents(inner, inner_names[b'hello']) == b'inner\n'
    assert contents(inner, inner_names[b'persist']) == b'tc-data'
    blank = contents(outer, names[b'blank'])
    expected = bytearray(578*512)
    expected[577*512:577*512+7] = b'tc-init'
    assert blank == expected


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
    log = Path(f'build/dectape-{ram}-boot{number}.log')
    log.write_bytes(output)
    required = [b'NEXIX_DECTAPE_OK', b'ROOT EXEC CLEANUP OK', b'BOOT OK']
    if owned:
        required.append(b'NEXIX_DECTAPE_IDE_OK')
    if number == 2:
        required.append(b'NEXIX_DECTAPE_PERSIST_OK')
    if not all(marker in output for marker in required) or b'PANIC' in output:
        raise SystemExit(f'FAIL: DECtape ATA boot {number} ({ram}); see {log}')
    if owned:
        check_media(disk)
    else:
        assert b'NEXIX_DECTAPE_IDE_OK' not in output


def main():
    with tempfile.TemporaryDirectory(prefix='nexix-dectape-') as directory:
        disk = Path(directory)/'auxiliary.img'
        volume = Image(2000)
        inner = Image(578)
        inner.inode(5, REG|0o644, b'inner\n')
        inner.directory(2, 2, [('hello', 5)])
        volume.inode(5, REG|0o600, tp_image())
        volume.inode(6, REG|0o600, inner.finish())
        volume.inode(7, REG|0o600)
        volume.directory(2, 2, [('tp', 5), ('fs', 6), ('blank', 7)])
        with disk.open('wb') as target:
            target.truncate(340670*512)
            target.write(b'DECTAPE TEST ONLY72')
            target.seek(5000*512)
            target.write(volume.finish())
        for number in (1, 2):
            boot(sys.argv[1], sys.argv[2], sys.argv[3], disk, number)
        with disk.open('r+b') as target:
            target.write(bytes(22))
        before = digest(disk)
        boot(sys.argv[1], sys.argv[2], sys.argv[3], disk, 3, owned=False)
        assert digest(disk) == before
    print(f'PASS: DECtape tp bytes/checksum, nested V7 mount/writes and fully initialized image across two cold boots; unmarked disk unchanged ({sys.argv[3]})')


if __name__ == '__main__':
    main()
