# Inspecting a running Nexix guest

Start a serial session with host debug access:

```sh
make run-debug
```

This boots normally and uses the usual persistent disk. For a VGA session,
use `make DEBUG=1 run`. Each debug session has local Unix sockets in
`build/debug/`, a QEMU pidfile and `qemu.log` (guest errors and CPU resets).
No TCP debugger port is opened. An active debug session prevents another debug
launch using the same directory. After it exits, the next launch archives that
directory with a UTC timestamp, preserving earlier captures.

If a problem occurs, leave QEMU running and ask for analysis. An agent working
in this same host workspace can connect to the monitor independently of the
serial terminal and guest shell. Mention the reproduction and whether the
console is VGA, COM1 or COM2. Do not rebuild the kernel before capturing: the
saved kernel ELF must match the running guest.

From a second **host** terminal, capture the running guest with:

```sh
make debug-capture
```

This briefly pauses the virtual CPU, collects registers, interrupt-controller
state, current instruction disassembly and a physical guest RAM ELF dump,
then resumes it if it was running. An already paused guest remains paused.
The printed `build/debug/capture-<UTC>/` directory contains:

- `state.json`: VM status, virtual CPU, registers, PIC/IRQ state and instructions.
- `memory.elf`: physical guest RAM, approximately the configured memory size.
- `nexix.elf`: a copy of the local kernel with its existing symbol table.
- `revision.txt`: the current Git commit; uncommitted changes are not recorded.
- `qemu.log`: the QEMU diagnostic log at capture time.

The dump is physical RAM, not a process core with automatically translated
virtual addresses. The kernel ELF provides function/global symbols; the normal
optimized build does not include C source/type debug information. Live GDB can
inspect the current virtual address space, registers, instructions and symbols:

```sh
gdb build/nexix.elf
```

Inside GDB:

```text
target remote build/debug/gdb.sock
info registers
x/16i $pc
bt
```

GDB attaching stops execution. Use `detach` to resume the guest before exiting.
Optimized kernel code and interrupt/context switches can limit backtraces.
GDB is optional for capture; install the host `gdb` package if you want to use it.

If the guest terminal hangs, Ctrl-A then C also switches the serial multiplexor
to QEMU's monitor. `info registers`, `info pic`, `stop` and `cont` are available
there. The independent QMP socket remains useful when terminal output is stuck.
A host-side QEMU hang can also block QMP; the capture command times out rather
than guaranteeing a snapshot in that case.

Use `DEBUG_DIR=build/debug-other` with both launch and capture to select another
session directory. Captures remain under `build/`, so copy them elsewhere before
`make clean`. `make test-debug-vm` verifies capture during `yes`, RAM/ELF output,
shell recovery, paused-state preservation and session archiving on disposable
16 MiB and 128 MiB guests.
