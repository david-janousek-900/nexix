"""Exercise V7 date/touch, timezone conversion and accounting on a fresh IDE root."""
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import tempfile
import time

ram = sys.argv[3] if len(sys.argv) > 3 else '128M'
output = bytearray()
position = 0
stage = 'startup'
with tempfile.TemporaryDirectory(prefix='nexix-time-') as directory:
    disk = Path(directory) / 'root.img'
    disk.write_bytes(Path('build/root.img').read_bytes())
    p = subprocess.Popen([sys.argv[1], '-machine','pc','-accel','tcg','-cpu','qemu64',
        '-smp','1','-m',ram,'-rtc','base=2024-02-29T12:34:56,clock=vm','-boot','d',
        '-drive',f'file={disk},format=raw,if=ide,index=0','-cdrom',sys.argv[2],
        '-display','none','-serial','stdio','-monitor','none','-no-reboot'],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    def expect(pattern):
        global position
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            match = re.search(pattern, output[position:])
            if match:
                position += match.end(); return
            if b'PANIC' in output or p.poll() is not None:
                raise RuntimeError('panic or QEMU exit')
            if select.select([p.stdout], [], [], .2)[0]:
                data = os.read(p.stdout.fileno(),4096)
                if not data: raise RuntimeError('closed console')
                output.extend(data)
        raise RuntimeError('timeout: ' + stage)
    def command(line, pattern):
        global stage
        stage = line
        p.stdin.write(line.encode() + b'\n'); p.stdin.flush()
        expect(rb'\r\n' + pattern + rb'\r\n# ')
    error = None
    try:
        expect(rb'NEXIX_BOOT_OK\r\n# ')
        command('date -u', rb'Thu Feb 29 12:34:[0-9]{2} GMT 2024')
        command('TZ=CET-1CEST,M3.5.0,M10.5.0/3; export TZ; date', rb'Thu Feb 29 13:34:[0-9]{2} CET 2024')
        command('date -u 2403310100.00', rb'Sun Mar 31 01:00:[0-9]{2} GMT 2024')
        command('date', rb'Sun Mar 31 03:00:[0-9]{2} CEST 2024')
        command('date 2403310400.00', rb'Sun Mar 31 04:00:[0-9]{2} CEST 2024')
        command('date -u', rb'Sun Mar 31 02:00:[0-9]{2} GMT 2024')
        command('date nonsense; echo STATUS_$?', rb'date: bad conversion\r\nSTATUS_1')
        command('date -u 2403312400', rb'Mon Apr  1 00:00:[0-9]{2} GMT 2024')
        command('touch /tmp/new; ls -l /tmp/new', rb'.*Apr  1 02:00 /tmp/new')
        command('echo preserved > /tmp/data; touch /tmp/data; cat /tmp/data', rb'preserved')
        command('touch -c /tmp/missing; ls /tmp', rb'touch: file /tmp/missing does not exist.\r\ndata\r\nnew')
        command('TZ=GMT0; export TZ; date', rb'Mon Apr  1 00:00:[0-9]{2} GMT 2024')
        command('date -u 0708', rb'Mon Apr  1 07:08:[0-9]{2} GMT 2024')
    except (RuntimeError, OSError) as problem:
        error = str(problem)
    finally:
        p.terminate()
        try: tail, _ = p.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            p.kill(); tail, _ = p.communicate()
        output.extend(tail)
    # Independent check of the native date accounting records in wtmp.
    if not error:
        data = disk.read_bytes()
        def u32(raw): return (int.from_bytes(raw[:2],'little') << 16) | int.from_bytes(raw[2:],'little')
        def node(ino): return data[1024+(ino-1)*64:1024+ino*64]
        def contents(ino):
            inode = node(ino); size = u32(inode[8:12]); value = bytearray()
            if size > 5120: raise RuntimeError('accounting fixture exceeds direct blocks')
            for index in range(10):
                raw = inode[12+index*3:15+index*3]
                block = (raw[0] << 16) | (raw[2] << 8) | raw[1]
                if block: value.extend(data[block*512:(block+1)*512])
            return bytes(value[:size])
        def lookup(path):
            ino = 2
            for name in path.strip('/').split('/'):
                entries = contents(ino)
                ino = next(int.from_bytes(entries[i:i+2],'little') for i in range(0,len(entries),16)
                           if entries[i+2:i+16].rstrip(b'\0').decode() == name)
            return ino
        accounting = contents(lookup('/usr/adm/wtmp'))
        if len(accounting) != 4 * 48 or any(accounting[i:i+1] != b'|' or accounting[i+24:i+25] != b'{' for i in range(0,len(accounting),48)):
            error = 'missing old/new time-setting accounting pairs'
        elif int.from_bytes(accounting[40:48],'little',signed=True) != 1711846800:
            error = 'UTC time-setting accounting value differs'
    log = Path('build') / f'time-{ram}.log'
    log.write_bytes(output)
    if error:
        sys.stdout.buffer.write(output)
        sys.exit(f'FAIL: {error}; see {log}')
print(f'PASS: {ram} V7 date UTC/local/DST/set/default fields, touch creation/content/-c and wtmp accounting')
