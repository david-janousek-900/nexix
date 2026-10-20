"""Sustained serial output must remain interruptible without losing other logins."""
from pathlib import Path
import re
import shutil
import sys
import tempfile
import time
from guest_acceptance import Guest
from multiuser_acceptance import login


def main():
    qemu, iso, ram = sys.argv[1:4]
    seconds = float(sys.argv[4]) if len(sys.argv) > 4 else 60
    if seconds <= 0:
        raise ValueError('duration must be positive')
    with tempfile.TemporaryDirectory(prefix='nx-yes-interrupt-') as temp:
        disk = Path(temp) / 'root.img'
        shutil.copyfile('build/distribution.img', disk)
        guest = Guest(qemu, iso, disk, ram, 2)
        try:
            login(guest, 'root', 'nexix')
            login(guest, 'alice', 'alice', 1, '$ ')
            for line in (0, 1):
                peer = 1 - line
                prompt = re.escape('# ' if line == 0 else '$ ')
                peer_prompt = re.escape('$ ' if peer == 1 else '# ')
                # Exercise both sustained output and a second interruption on
                # the same tty after the first foreground process was reaped.
                for duration in (seconds, 2):
                    guest.send('yes\n', line)
                    guest.expect('yes\r\n', line)
                    total = lines = 0
                    deadline = time.monotonic() + duration
                    while time.monotonic() < deadline:
                        guest.pump(.02)
                        data = guest.buffers[line]
                        if b'PANIC' in data:
                            raise AssertionError('kernel panic during yes')
                        total += len(data)
                        lines += data.count(b'y\r\n')
                        # Bound host memory without throttling the UART or
                        # reaching Guest's normal 2 MiB transcript limit.
                        data.clear()
                        guest.positions[line] = 0
                    assert lines > 100, ('yes did not produce sustained output', line, total)
                    guest.send('\x03', line)
                    guest.expect(prompt, line, timeout=10)
                    guest.command('echo INTERRUPT_RECOVERED', line,
                                  stdout='INTERRUPT_RECOVERED\n', prompt=prompt)
                    guest.command('echo OTHER_SESSION_ALIVE', peer,
                                  stdout='OTHER_SESSION_ALIVE\n', prompt=peer_prompt)
                    print(f'PASS: {ram} COM{line + 1}, {duration:g}s yes, {total} bytes, '
                          'Ctrl-C, shell recovery and other-session liveness', flush=True)
        finally:
            for line, output in enumerate(guest.buffers):
                Path(f'build/yes-interrupt-{ram}.tty{line}.log').write_bytes(output)
            guest.diagnostics.seek(0)
            Path(f'build/yes-interrupt-{ram}.qemu.log').write_bytes(guest.diagnostics.read())
            guest.close()


if __name__ == '__main__':
    main()
