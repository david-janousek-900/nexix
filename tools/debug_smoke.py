"""Verify debug capture from outside an unresponsive guest terminal."""
import json
from pathlib import Path
import shutil
import sys
import tempfile
from debug_vm import Monitor, capture, prepare
from guest_acceptance import Guest
from multiuser_acceptance import login

qemu, iso, ram = sys.argv[1:4]
with tempfile.TemporaryDirectory(prefix='nx-debug-test-') as temp:
    directory = Path(temp) / 'debug'
    prepare(directory)
    disk = Path(temp) / 'root.img'
    shutil.copyfile('build/distribution.img', disk)
    extra = ['-qmp', f'unix:{directory}/qmp.sock,server=on,wait=off',
             '-gdb', f'unix:{directory}/gdb.sock,server=on,wait=off',
             '-pidfile', str(directory / 'qemu.pid'),
             '-D', str(directory / 'qemu.log'), '-d', 'guest_errors,cpu_reset']
    guest = Guest(qemu, iso, disk, ram, 2, extra_args=extra)
    try:
        login(guest, 'root', 'nexix')
        assert (directory / 'gdb.sock').exists()
        try:
            prepare(directory)
        except RuntimeError:
            pass
        else:
            raise AssertionError('prepare accepted a live debug session')
        # Capture without reading or issuing any command to the guest shell.
        guest.send('yes\n')
        guest.expect('y\r\n')
        saved = capture(directory)
        report = json.loads((saved / 'state.json').read_text())
        assert report['before']['running'] and not report['stopped']['running']
        assert 'RIP=' in report['info registers']
        assert (saved / 'memory.elf').read_bytes()[:4] == b'\x7fELF'
        assert (saved / 'memory.elf').stat().st_size > 1024 * 1024 * int(ram[:-1])
        assert (saved / 'nexix.elf').read_bytes() == Path('build/nexix.elf').read_bytes()
        guest.send('\x03'); guest.expect('# ')
        guest.command('echo DEBUG_RESUMED', stdout='DEBUG_RESUMED\n')
        monitor = Monitor(directory / 'qmp.sock')
        try:
            monitor.call('stop')
        finally:
            monitor.close()
        saved = capture(directory)
        monitor = Monitor(directory / 'qmp.sock')
        try:
            assert not monitor.call('query-status')['running'], 'capture resumed an already paused guest'
            monitor.call('cont')
        finally:
            monitor.close()
        guest.command('echo DEBUG_ALIVE', stdout='DEBUG_ALIVE\n')
    finally:
        guest.close()
    prepare(directory)
    assert not any(directory.iterdir()), 'previous debug session was not archived'
    assert list(Path(temp).glob('debug-*/capture-*')), 'archive lost captures'
print(f'PASS: {ram} live QMP capture, registers/RAM/symbols, resume, paused state and session preservation')
