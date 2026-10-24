"""Inject NMI on a running guest and verify the shell continues executing."""
import json
import select
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

with tempfile.TemporaryDirectory() as directory:
    path = directory + '/qmp'
    process = subprocess.Popen([sys.argv[1], '-machine', 'pc', '-accel', 'tcg',
        '-cpu', 'qemu64,+rdrand', '-m', '128M', '-cdrom', sys.argv[2], '-display', 'none',
        '-serial', 'stdio', '-qmp', 'unix:'+path+',server=on,wait=off', '-no-reboot'],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = bytearray()
    def until(marker):
        deadline = time.monotonic()+45
        while marker not in output:
            if time.monotonic()>deadline or process.poll() is not None:
                raise RuntimeError('guest failed to reach '+repr(marker))
            if select.select([process.stdout], [], [], .2)[0]:
                output.extend(process.stdout.read1(65536))
    try:
        until(b'# ')
        connection=socket.socket(socket.AF_UNIX)
        connection.connect(path)
        stream=connection.makefile('rwb', buffering=0)
        assert 'QMP' in json.loads(stream.readline())
        def command(name):
            stream.write((json.dumps({'execute':name})+'\n').encode())
            while True:
                reply=json.loads(stream.readline())
                if 'return' in reply:return
                if 'error' in reply:raise RuntimeError(reply)
        command('qmp_capabilities')
        for _ in range(3):command('inject-nmi')
        process.stdin.write(b'echo NMI-SURVIVED\n');process.stdin.flush()
        until(b'NMI-SURVIVED\r\nroot@nexix.localdomain# ')
        assert b'PANIC' not in output
        print('PASS: repeated injected NMI returns safely and shell remains usable')
    finally:
        process.terminate()
        process.communicate(timeout=5)
        Path('build/nmi-boot.log').write_bytes(output)
