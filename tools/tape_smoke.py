"""Cold-boot a disposable primary-slave ATA disk through sequential HT/TM files."""
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


def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte)*16777619) & 0xffffffff
    return value


def check_media(path):
    with path.open('rb') as source:
        source.seek(5000*512)
        image = source.read(2000*512)
    entries = contents(image, 2)
    files = {entries[i+2:i+16].rstrip(b'\0'): struct.unpack_from('<H', entries, i)[0]
             for i in range(0, len(entries), 16)}
    tape = contents(image, files[b'image'])
    assert tape[:8] == b'NXTAPE1\0'
    version, header, end, records, capacity, generation = struct.unpack_from('<IIQQQQ', tape, 8)
    assert (version, header, records, capacity) == (1, 64, 3, 65536)
    assert generation > 0 and len(tape) >= end
    assert checksum(tape[:48]+tape[52:64]) == struct.unpack_from('<I', tape, 48)[0]
    cursor = 64
    decoded = []
    while cursor < end:
        length, crc = struct.unpack_from('<II', tape, cursor)
        payload = tape[cursor+8:cursor+8+length]
        footer = cursor+8+((length+1)&~1)
        assert tape[cursor:cursor+8] == tape[footer:footer+8]
        assert crc == (checksum(payload) if length else 0)
        decoded.append(payload)
        cursor = footer+8
    assert cursor == end and decoded == [b'tape-persist', b'', b'']


def boot(qemu, iso, ram, disk, number, owned=True):
    command = [qemu, '-machine', 'pc', '-accel', 'tcg', '-cpu', 'qemu64,+rdrand', '-smp', '1',
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
    log = Path(f'build/tape-{ram}-boot{number}.log')
    log.write_bytes(output)
    required = [b'NEXIX_TAPE_OK', b'ROOT EXEC CLEANUP OK', b'BOOT OK']
    if owned:
        required.append(b'NEXIX_TAPE_IDE_OK')
    if number == 2:
        required.append(b'NEXIX_TAPE_PERSIST_OK')
    if not all(marker in output for marker in required) or b'PANIC' in output:
        raise SystemExit(f'FAIL: tape ATA boot {number} ({ram}); see {log}')
    if owned:
        check_media(disk)
    else:
        assert b'NEXIX_TAPE_IDE_OK' not in output


def main():
    with tempfile.TemporaryDirectory(prefix='nexix-tape-') as directory:
        disk = Path(directory)/'auxiliary.img'
        volume = Image(2000)
        volume.directory(2, 2, [])
        with disk.open('wb') as target:
            target.truncate(340670*512)
            target.write(b'TAPE TEST ONLY71')
            target.seek(5000*512)
            target.write(volume.finish())
        for number in (1, 2):
            boot(sys.argv[1], sys.argv[2], sys.argv[3], disk, number)
        with disk.open('r+b') as target:
            target.write(bytes(22))
        before = digest(disk)
        boot(sys.argv[1], sys.argv[2], sys.argv[3], disk, 3, owned=False)
        assert digest(disk) == before
    print(f'PASS: sequential tape image, records/two marks and header/record checksums across two cold boots; unmarked disk unchanged ({sys.argv[3]})')


if __name__ == '__main__':
    main()
