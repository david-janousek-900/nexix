"""Check boot/timer or deliberate fault diagnostics in a bounded QEMU run."""
import re
import subprocess
import sys
from pathlib import Path

mode = sys.argv[3] if len(sys.argv) > 3 else "boot"
user_faults = {"user-kernel": (14, 5), "user-ro": (14, 7), "user-nx": (14, 21),
               "user-priv": (13, 0), "user-io": (13, 0), "user-int": (13, 258),
               "user-df": (8, 0)}
if mode not in ("boot", "ud", "pf", "memory", "wp", "nx", "unmap", "null",
                "paging-oom", "protect", "user-bad-stack") and mode not in user_faults:
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
if mode in ("boot", "memory", "paging-oom", "user-bad-stack"):
    if (b"NEXIX_BOOT_OK\r\n" not in output or b"NEXIX_TIMER_OK\r\n" not in output
            or b"NEXIX_MEMORY_OK\r\n" not in output
            or b"NEXIX_PAGING_OK\r\n" not in output or b"PANIC" in output
            or any(marker not in output for marker in
                   (b"NEXIX_USER_MODE_OK\r\n", b"NEXIX_USER_STACK_OK\r\n",
                    b"NEXIX_USER_TIMER_OK\r\n", b"NEXIX_USER_RETURN_OK\r\n"))):
        sys.exit(f"FAIL: boot/timer check failed; see {log}")
    if mode == "memory" and (b"NEXIX_MEMORY_EXHAUSTION_OK\r\n" not in output
                             or b"NEXIX_RESERVATIONS_OK\r\n" not in output):
        sys.exit(f"FAIL: memory exhaustion check failed; see {log}")
    if mode == "paging-oom" and b"NEXIX_PAGING_OOM_OK\r\n" not in output:
        sys.exit(f"FAIL: page-table allocation rollback failed; see {log}")
    if mode == "user-bad-stack" and b"NEXIX_USER_BAD_STACK_OK\r\n" not in output:
        sys.exit(f"FAIL: TSS handoff with unusable user stack failed; see {log}")
    print("PASS: long-mode boot and repeated timer interrupt return")
elif mode in user_faults:
    vector, error = user_faults[mode]
    fault = re.search(rb"exception vector=(0x[0-9a-f]{16}) error=(0x[0-9a-f]{16})"
                      rb" rip=0x[0-9a-f]{16}(?: cr2=(0x[0-9a-f]{16}))?\r\n"
                      rb"Nexix PANIC: unhandled CPU exception\r\n", output)
    if (not fault or int(fault[1], 16) != vector or int(fault[2], 16) != error
            or b"NEXIX_BOOT_OK" in output):
        sys.exit(f"FAIL: {mode} exception incorrect; see {log}")
    if mode == "user-df":
        if b"NEXIX_DOUBLE_FAULT_STACK_OK\r\n" not in output:
            sys.exit(f"FAIL: double-fault IST stack not validated; see {log}")
    else:
        if not re.search(rb"trap cs=0x0000000000000023 ss=0x000000000000001b", output):
            sys.exit(f"FAIL: exception did not originate in user mode; see {log}")
        if vector == 14:
            expected = re.search(rb"expected cr2=(0x[0-9a-f]{16})\r\n", output)
            if not expected or fault[3] != expected[1]:
                sys.exit(f"FAIL: incorrect user fault address; see {log}")
    print(f"PASS: {mode} user protection/entry stack diagnostics")
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
