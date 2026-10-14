"""Check boot/timer or deliberate fault diagnostics in a bounded QEMU run."""
import re
import os
import select
import time
import subprocess
import sys
from pathlib import Path

mode = sys.argv[3] if len(sys.argv) > 3 else "boot"
user_faults = {"user-kernel": (14, 5), "user-ro": (14, 7), "user-nx": (14, 21),
               "user-priv": (13, 0), "user-io": (13, 0), "user-int": (13, 258),
               "user-df": (8, 0)}
if mode not in ("boot", "process", "syscall", "exec", "libc", "fs", "files", "signals", "v7", "rootfs", "tty", "tty-demo", "ud", "pf", "memory", "wp", "nx", "unmap", "null",
                "paging-oom", "protect", "user-bad-stack") and mode not in user_faults:
    sys.exit("FAIL: unknown smoke mode")
ram = sys.argv[4] if len(sys.argv) > 4 else "128M"
command = [sys.argv[1], "-machine", "pc", "-accel", "tcg", "-cpu", "qemu64",
           "-smp", "1", "-m", ram, "-cdrom", sys.argv[2], "-display", "none",
           "-serial", "stdio", "-monitor", "none", "-no-reboot"]
try:
    process = subprocess.Popen(command, stdin=subprocess.PIPE if mode in ("tty", "tty-demo") else None,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
except FileNotFoundError:
    sys.exit("FAIL: QEMU is not installed")
if mode in ("tty", "tty-demo"):
    # Exchange bytes through QEMU's COM1 chardev, without kernel input injection.
    replies = {b"NEXIX_TTY_UART_COOKED_READY\r\n": b"heX\bllo\r",
               b"NEXIX_GETPASS_1_READY%\r\n": b"\x03secret99EXCESS\r",
               b"NEXIX_GETPASS_2_READY\r\n": b"two\r",
               b"NEXIX_GETPASS_EOF_READY\r\n": b"\x04",
               b"NEXIX_TTY_UART_RAW_READY\r\n": b"\x00\x80\xffR",
               b"NEXIX_TTY_UART_CBREAK_READY\r\n": b"c",
               b"NEXIX_TTY_UART_STOP_READY\r\n": b"\x13s",
               b"NEXIX_TTY_UART_START_READY\r\n": b"\x11",
               b"NEXIX_TTY_UART_INTR_READY\r\n": b"\x03",
               b"NEXIX_TTY_UART_QUIT_READY\r\n": b"\x1c"} if mode == "tty" else {
                   b"NEXIX_TTY_DEMO_READY\r\n": b"terminal demo\r",
                   b"received: terminal demo\r\n": b"quit\r"}
    output = bytearray()
    deadline = time.monotonic() + 45
    while process.poll() is None and time.monotonic() < deadline:
        ready, _, _ = select.select([process.stdout], [], [], 0.2)
        if not ready:
            continue
        chunk = os.read(process.stdout.fileno(), 4096)
        if not chunk:
            break
        output.extend(chunk)
        for marker in list(replies):
            if marker in output:
                process.stdin.write(replies.pop(marker))
                process.stdin.flush()
    exited_early = process.poll() is not None
    process.terminate()
    try:
        remaining, _ = process.communicate(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        remaining, _ = process.communicate()
    output = bytes(output) + remaining
else:
    try:
        output, _ = process.communicate(timeout=45 if mode in ("boot", "process", "syscall", "exec", "libc", "fs", "files", "signals", "v7", "rootfs") else 15)
        exited_early = True
    except subprocess.TimeoutExpired:
        exited_early = False
        process.terminate()
        try:
            output, _ = process.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            output, _ = process.communicate()
log = Path("build") / (f"memory-{ram}.log" if mode == "memory" else
                        f"{mode}-{ram}.log" if mode in ("process", "syscall", "exec", "libc", "fs", "files", "signals", "v7", "rootfs", "tty", "tty-demo") else f"{mode}.log")
log.write_bytes(output)
sys.stdout.buffer.write(output)
if mode == "tty" and (b"secret99" in output or b"EXCESS" in output or b"NEXIX_GETPASS_OK" not in output):
    sys.exit(f"FAIL: getpass echo suppression or serial acceptance; see {log}")
if exited_early:
    sys.exit(f"FAIL: QEMU exited unexpectedly ({process.returncode}); see {log}")
if mode in ("boot", "process", "syscall", "exec", "libc", "fs", "files", "signals", "v7", "rootfs", "tty", "tty-demo", "memory", "paging-oom", "user-bad-stack"):
    if (b"NEXIX_BOOT_OK\r\n" not in output or b"NEXIX_TIMER_OK\r\n" not in output
            or b"NEXIX_MEMORY_OK\r\n" not in output
            or b"NEXIX_PAGING_OK\r\n" not in output or b"PANIC" in output
            or any(marker not in output for marker in
                   (b"NEXIX_USER_MODE_OK\r\n", b"NEXIX_USER_STACK_OK\r\n",
                    b"NEXIX_USER_TIMER_OK\r\n", b"NEXIX_USER_RETURN_OK\r\n"))):
        sys.exit(f"FAIL: boot/timer check failed; see {log}")
    process_markers = ("SWITCH", "PREEMPT", "SLEEP", "WAIT", "SLOTS", "OOM", "FORK", "CLEANUP")
    if any(f"NEXIX_PROCESS_{marker}_OK\r\n".encode() not in output for marker in process_markers):
        sys.exit(f"FAIL: process scheduling/lifecycle check failed; see {log}")
    syscall_markers = ("NEXIX_SYSCALL_ABI_OK", "NEXIX_UACCESS_OK", "NEXIX_SYSCALL_ERRORS_OK",
                       "NEXIX_SYSCALL_FORK_OK", "NEXIX_SYSCALL_OK")
    if any(f"{marker}\r\n".encode() not in output for marker in syscall_markers):
        sys.exit(f"FAIL: native syscall/user-copy check failed; see {log}")
    if any(f"{marker}\r\n".encode() not in output for marker in
           ("NEXIX_ELF_OK", "NEXIX_EXEC_ROLLBACK_OK", "NEXIX_EXEC_OK")):
        sys.exit(f"FAIL: ELF/exec check failed; see {log}")
    if any(f"{marker}\r\n".encode() not in output for marker in
           ("NEXIX_LIBC_ABI_OK", "NEXIX_LIBC_OK", "NEXIX_FPU_OK", "NEXIX_FPU_USER_OK", "NEXIX_FPU_PREEMPT_OK")):
        sys.exit(f"FAIL: native libc check failed; see {log}")
    if any(f"{marker}\r\n".encode() not in output for marker in
           ("NEXIX_BLOCK_OK", "NEXIX_V7FS_READ_OK", "NEXIX_V7FS_ALLOC_OK",
            "NEXIX_V7FS_WRITE_OK", "NEXIX_V7FS_EXEC_OK", "NEXIX_V7FS_OK")):
        sys.exit(f"FAIL: block/V7 filesystem check failed; see {log}")
    if any(f"{marker}\r\n".encode() not in output for marker in
           ("NEXIX_FILES_OK", "NEXIX_PIPE_OK", "NEXIX_FILES_CLEANUP_OK")):
        sys.exit(f"FAIL: file descriptors/pipes/ownership check failed; see {log}")
    if any(f"{marker}\r\n".encode() not in output for marker in
           ("NEXIX_TTY_USER_WRITE_OK", "NEXIX_TTY_OK", "NEXIX_TTY_CLEANUP_OK")):
        sys.exit(f"FAIL: serial terminal/ioctl/ownership check failed; see {log}")
    if any(f"{marker}\r\n".encode() not in output for marker in
           ("NEXIX_SIGNALS_OK", "NEXIX_HEAP_OK", "NEXIX_INIT_SYSCALLS_OK", "NEXIX_SIGNALS_CLEANUP_OK")):
        sys.exit(f"FAIL: signals/heap/init syscalls check failed; see {log}")
    if any(f"{marker}\r\n".encode() not in output for marker in
           ("NEXIX_V7_INIT_ELF_OK", "NEXIX_V7_USER_PROGRAM_OK", "NEXIX_V7_SHELL_OK", "NEXIX_V7_UTILITIES_OK", "NEXIX_V7_USERLAND_CLEANUP_OK")):
        sys.exit(f"FAIL: rebuilt V7 userland check failed; see {log}")
    if any(f"{marker}\r\n".encode() not in output for marker in
           ("NEXIX_ROOT_IMAGE_OK", "NEXIX_ROOT_ELF_OK", "NEXIX_ROOT_EXEC_USER_OK", "NEXIX_STORAGE_OK", "NEXIX_NAMESPACE_OK", "NEXIX_UMOUNT_OK", "NEXIX_MOUNT_EXEC_OK", "NEXIX_ROOT_EXEC_OK", "NEXIX_ROOT_EXEC_CLEANUP_OK")):
        sys.exit(f"FAIL: packaged root image check failed; see {log}")
    if mode == "tty" and (replies or b"NEXIX_TTY_UART_OK\r\n" not in output
                           or b"NEXIX_TTY_UART_USER_OK\r\n" not in output
                           or b"NEXIX_TTY_UART_RESUMED\r\n" not in output
                           or b"NEXIX_TTY_UART_SIGNALS_OK\r\n" not in output):
        sys.exit(f"FAIL: COM1 IRQ input/flow-control check failed; see {log}")
    if mode == "tty-demo" and (replies or b"NEXIX_TTY_DEMO_OK\r\n" not in output
                                or b"received: terminal demo\r\n" not in output):
        sys.exit(f"FAIL: interactive terminal demo check failed; see {log}")
    if mode == "memory" and (b"NEXIX_MEMORY_EXHAUSTION_OK\r\n" not in output
                             or b"NEXIX_RESERVATIONS_OK\r\n" not in output):
        sys.exit(f"FAIL: memory exhaustion check failed; see {log}")
    if mode == "paging-oom" and b"NEXIX_PAGING_OOM_OK\r\n" not in output:
        sys.exit(f"FAIL: page-table allocation rollback failed; see {log}")
    if mode == "user-bad-stack" and b"NEXIX_USER_BAD_STACK_OK\r\n" not in output:
        sys.exit(f"FAIL: TSS handoff with unusable user stack failed; see {log}")
    print("PASS: boot, scheduling, fork/exit/wait, native syscalls, user copies, ELF exec, native libc, V7 filesystem, file descriptors, pipes, serial TTY, signals, heap, init services, rebuilt V7 userland, and packaged root image")
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
