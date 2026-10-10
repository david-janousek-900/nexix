"""Check boot/timer or deliberate fault diagnostics in a bounded QEMU run."""
import re
import subprocess
import sys
from pathlib import Path

mode = sys.argv[3] if len(sys.argv) > 3 else "boot"
if mode not in ("boot", "ud", "pf", "memory", "wp", "nx", "unmap", "null", "paging-oom", "protect"):
    sys.exit("FAIL: unknown smoke mode")
ram = sys.argv[4] if len(sys.argv) > 4 else "128M"
command = [sys.argv[1], "-machine", "pc", "-accel", "tcg", "-cpu", "qemu64",
           "-smp", "1", "-m", ram, "-cdrom", sys.argv[2], "-display", "none",
           "-serial", "stdio", "-monitor", "none", "-no-reboot"]
try:
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
except FileNotFoundError:
    sys.exit("FAIL: QEMU is not installed")
try:
    output, _ = process.communicate(timeout=15)
    exited_early = True
except subprocess.TimeoutExpired:
    exited_early = False
    process.terminate()
    try:
        output, _ = process.communicate(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        output, _ = process.communicate()
log = Path("build") / (f"memory-{ram}.log" if mode == "memory" else f"{mode}.log")
log.write_bytes(output)
sys.stdout.buffer.write(output)
if exited_early:
    sys.exit(f"FAIL: QEMU exited unexpectedly ({process.returncode}); see {log}")
if mode in ("boot", "memory", "paging-oom"):
    if (b"NEXIX_BOOT_OK\r\n" not in output or b"NEXIX_TIMER_OK\r\n" not in output
            or b"NEXIX_MEMORY_OK\r\n" not in output
            or b"NEXIX_PAGING_OK\r\n" not in output or b"PANIC" in output):
        sys.exit(f"FAIL: boot/timer check failed; see {log}")
    if mode == "memory" and (b"NEXIX_MEMORY_EXHAUSTION_OK\r\n" not in output
                             or b"NEXIX_RESERVATIONS_OK\r\n" not in output):
        sys.exit(f"FAIL: memory exhaustion check failed; see {log}")
    if mode == "paging-oom" and b"NEXIX_PAGING_OOM_OK\r\n" not in output:
        sys.exit(f"FAIL: page-table allocation rollback failed; see {log}")
    print("PASS: long-mode boot and repeated timer interrupt return")
elif mode in ("wp", "nx", "unmap", "null", "protect"):
    expected = re.search(rb"expected cr2=(0x[0-9a-f]{16})\r\n", output)
    error = {"wp": 3, "nx": 17, "unmap": 0, "null": 0, "protect": 3}[mode]
    fault = re.search(rb"exception vector=0x000000000000000e error=(0x[0-9a-f]{16})"
                      rb" rip=0x[0-9a-f]{16} cr2=(0x[0-9a-f]{16})\r\n"
                      rb"Nexix PANIC: unhandled CPU exception\r\n", output)
    if (not expected or not fault or int(fault[1], 16) != error
            or fault[2] != expected[1] or b"NEXIX_BOOT_OK" in output
            or b"NEXIX_PAGING_OK\r\n" not in output):
        sys.exit(f"FAIL: {mode} protection fault incorrect; see {log}")
    print(f"PASS: {mode} page protection and fault diagnostics")
else:
    vector = b"6" if mode == "ud" else b"e"
    pattern = (rb"exception vector=0x000000000000000" + vector
               + rb" error=0x0000000000000000 rip=0x[0-9a-f]{16}"
               + (rb" cr2=0x0000000040000000" if mode == "pf" else b"")
               + rb"\r\nNexix PANIC: unhandled CPU exception\r\n")
    if not re.search(pattern, output) or b"NEXIX_BOOT_OK" in output:
        sys.exit(f"FAIL: {mode} fault diagnostics incorrect; see {log}")
    print(f"PASS: {mode} exception frame and panic diagnostics")
