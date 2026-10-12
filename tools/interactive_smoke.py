"""Accept the running V7 shell using real COM1 input, without test gates."""
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import time
import tempfile

ram = sys.argv[3] if len(sys.argv) > 3 else '128M'
disk_directory = tempfile.TemporaryDirectory(prefix='nexix-shell-')
ide = len(sys.argv) > 4 and sys.argv[4] == 'ide'
extra = []
if ide:
    disk = Path(disk_directory.name) / 'root.img'
    disk.write_bytes(Path('build/root.img').read_bytes())
    extra = ['-boot', 'd', '-drive', f'file={disk},format=raw,if=ide,index=0']
try:
    process = subprocess.Popen([sys.argv[1], '-machine', 'pc', '-accel', 'tcg',
        '-cpu', 'qemu64', '-smp', '1', '-m', ram, '-cdrom', sys.argv[2],
        '-display', 'none', '-serial', 'stdio', '-monitor', 'none', '-no-reboot'] + extra,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
except FileNotFoundError:
    sys.exit('FAIL: QEMU is not installed')
output = bytearray()
position = 0
stage = 'startup'
checks = 0
error = None

def send(data):
    process.stdin.write(data)
    process.stdin.flush()

def expect(pattern):
    global position
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if b'PANIC' in output:
            raise RuntimeError('kernel panic')
        match = re.search(pattern, output[position:])
        if match:
            position += match.end()
            return match
        if process.poll() is not None:
            raise RuntimeError('QEMU exited unexpectedly')
        ready, _, _ = select.select([process.stdout], [], [], 0.2)
        if ready:
            data = os.read(process.stdout.fileno(), 4096)
            if not data:
                raise RuntimeError('COM1 closed')
            output.extend(data)
    raise RuntimeError(f'timeout in {stage}')

def command(line, result=b''):
    global checks
    send(line if line.endswith(b'\r') else line + b'\n')
    expect(rb'\r\n' + re.escape(result.replace(b'\n', b'\r\n')) + rb'# ')
    checks += 1

def cat_ready(name, pipeline=False):
    # V7 cat explicitly buffers stdout; -u is required for a live round trip.
    line = b'cat -u | cat -u' if pipeline else b'cat -u'
    send(line + b'\n')
    expect(re.escape(line + b'\r\n'))
    send(name + b'\n')
    # UART echo followed by cat's output proves the child has started reading.
    expect(re.escape(name + b'\r\n' + name + b'\r\n'))

try:
    expect(rb'NEXIX_INIT_PID1\r\nNEXIX_BOOT_OK\r\n# ')
    stage = 'empty prompt lines'
    for _ in range(10):
        command(b'')
        command(b'   ')
        command(b'# comment only')
    stage = 'queued empty lines'
    send(b'\n' * 32)
    for _ in range(32):
        expect(rb'# ')
    command(b'echo EMPTY_LINES_OK', b'EMPTY_LINES_OK\n')
    stage = 'line editing'
    command(b'echo EDIT_X\bY\r', b'EDIT_Y\n')
    command(b'echo DEL_X\x7fY', b'DEL_Y\n')
    command(b'echo discarded\x15echo KILL_OK', b'KILL_OK\n')
    stage = 'shell language'
    command(b'V="two words"; export V; /bin/sh -c \'echo "$V"\'; echo "`echo sub`"', b'two words\nsub\n')
    command(b'for I in a b c; do echo $I; done', b'a\nb\nc\n')
    command(b'if /bin/sh -c "exit 7"; then echo bad; else echo good; fi', b'good\n')
    command(b'echo first | cat | cat', b'first\n')
    command(b'(echo sub-shell); echo parent', b'sub-shell\nparent\n')
    stage = 'exit status and background wait'
    command(b'/bin/sh -c "exit 7"; echo STATUS_$?', b'STATUS_7\n')
    send(b'nexix_missing\n'); expect(rb'nexix_missing: not found\r\n# ')
    command(b'echo STATUS_$?', b'STATUS_1\n')
    send(b"/bin/sh -c 'exit 0' & wait; echo WAIT_OK\n")
    expect(rb'\r\n[0-9]+\r\nWAIT_OK\r\n# ')
    checks += 1
    send(b'cat & wait; echo BG_CAT_OK\n')
    expect(rb'\r\n[0-9]+\r\nBG_CAT_OK\r\n# ')
    checks += 1
    stage = 'here document'
    send(b'cat <<END\n'); expect(rb'> ')
    send(b'heredoc $V\n'); expect(rb'> ')
    send(b'END\n'); expect(rb'\r\nheredoc two words\r\n# ')
    checks += 1
    stage = 'files and scripts'
    command(b'echo first > /tmp/a; echo second >> /tmp/a; cp /tmp/a /tmp/b; ln /tmp/b /tmp/c; cat /tmp/c', b'first\nsecond\n')
    command(b'cd /tmp; pwd; echo /tmp/[abc]', b'/tmp\n/tmp/a /tmp/b /tmp/c\n')
    command(b"echo 'echo SCRIPT_OK' > script; . ./script", b'SCRIPT_OK\n')
    command(b'echo fourteen > abcdefghijklmn; cat abcdefghijkl*', b'fourteen\n')
    command(b'rm a b c script abcdefghijklmn; cd /; echo CLEAN_OK', b'CLEAN_OK\n')
    stage = 'idle interrupt'
    send(b'echo discarded'); expect(rb'echo discarded')
    send(b'\x03'); expect(rb'\r\n# ')
    command(b'echo IDLE_PID_$$', b'IDLE_PID_2\n')
    stage = 'foreground interrupts'
    for i in range(3):
        cat_ready(f'CAT_READY_{i}'.encode())
        send(b'\x03'); expect(rb'\r\n# ')
        command(b'echo INTERRUPT_PID_$$', b'INTERRUPT_PID_2\n')
    cat_ready(b'QUIT_READY')
    send(b'\x1c'); expect(rb'Quit - core dumped\r\n# ')
    command(b'ls core', b'core\n')
    command(b'rm core')
    command(b'echo QUIT_PID_$$', b'QUIT_PID_2\n')
    stage = 'pipeline interrupt and EOF'
    cat_ready(b'PIPE_READY', pipeline=True)
    send(b'\x03'); expect(rb'\r\n# ')
    command(b'echo PIPE_PID_$$', b'PIPE_PID_2\n')
    send(b'cat -u\n'); expect(rb'cat -u\r\n')
    send(b'EOF_LINE\x04'); expect(rb'EOF_LINEEOF_LINE')
    send(b'\x04'); expect(rb'# ')
    command(b'echo EOF_PID_$$', b'EOF_PID_2\n')
    stage = 'buffered cat EOF flush'
    send(b'cat\n'); expect(rb'cat\r\n')
    send(b'BUFFERED_CAT\n'); expect(rb'BUFFERED_CAT\r\n')
    send(b'\x04'); expect(rb'BUFFERED_CAT\r\n# ')
    checks += 1
    stage = 'trap delivery'
    command(b"trap 'echo TRAP_OK' 2; echo TRAP_SET", b'TRAP_SET\n')
    cat_ready(b'TRAP_READY')
    send(b'\x03'); expect(rb'TRAP_OK\r\n# ')
    command(b'trap 2; echo TRAP_CLEAR', b'TRAP_CLEAR\n')
    stage = 'long expansion'
    command(b'L=; echo LONG_START', b'LONG_START\n')
    for _ in range(40):
        command(b'L="${L}' + b'x' * 128 + b'"')
    command(b'echo "$L"', b'x' * 5120 + b'\n')
    command(b'unset L; echo LONG_DONE', b'LONG_DONE\n')
    stage = 'repeated fork/exec/wait'
    for i in range(200):
        marker = f'SESSION_{i:02d}'.encode()
        command(b'echo ' + marker, marker + b'\n')
    command(b'echo FINAL_PID_$$; ls /tmp', b'FINAL_PID_2\n')
    stage = 'carriage returns'
    for _ in range(10):
        command(b'\r')
    send(b'\r\n')
    expect(rb'# ')
    expect(rb'# ')
    stage = 'shell EOF and restart'
    for _ in range(3):
        send(b'\x04')
        expect(rb'# ')
        command(b'echo RESTART_OK', b'RESTART_OK\n')
except (RuntimeError, BrokenPipeError) as problem:
    error = str(problem)
finally:
    exited_early = process.poll() is not None
    process.terminate()
    try:
        remaining, _ = process.communicate(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        remaining, _ = process.communicate()
    output.extend(remaining)
log = Path('build') / (f'interactive-{ram}' + ('-ide.log' if ide else '.log'))
log.write_bytes(output)
disk_directory.cleanup()
if not error and (exited_early or b'PANIC' in output):
    error = 'unexpected QEMU exit or kernel panic'
if error:
    sys.stdout.buffer.write(output[-8192:])
    sys.exit(f'FAIL: {error}; see {log}')
print(f'PASS: {ram} interactive V7 shell, {checks} command checks; see {log}')
