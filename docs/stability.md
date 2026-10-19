# Stability investigation

Task 365 fixes a concrete sleep lost-wakeup race: SIGALRM could arrive after
alarm setup but before pause, leaving the process waiting indefinitely. A
deterministic host regression injects that ordering; native tests also exercise
short deadline boundaries and child-delivered signals.

The reported intermittent VGA/serial freeze is still under investigation.
Passing bounded acceptance runs do not establish that it has been fixed.

Run `make test-stability` for 100 rounds on each of the 16 and 128 MiB
configurations, or `make test-stability STABILITY_ROUNDS=1000` for a longer run.
Each round checks hostname queries, a pipeline through two children, and
filesystem reads/writes. Two independent logins stay active, and periodic idle
intervals exercise scheduler halt and UART wakeup. Tests create disposable disks;
they never modify `build/nexix.img`. Serial and QEMU logs are retained in
`build/stability-<RAM>.tty0.log`, `.tty1.log` and `.qemu.log`, including on failure.
Use `make test-vga` for actual PS/2 input and VGA display acceptance.

The full boot self-test installs temporary native executables and writes core
images. It now extends its private RAM volume to 1 MiB before these tests;
the shipped recovery image and ordinary recovery filesystem remain 512 KiB.
The block-device boundary tests read their expected size from the superblock.
This addresses test scratch-space exhaustion, rather than reducing the checks.

For an unrecoverable lockup, record the exact launch command or libvirt XML,
RAM/CPU count, emulator and accelerator, attached console devices, last command,
and whether the guest was idle. Nexix currently targets one CPU, BIOS/i440fx,
IDE storage, and VGA/PS/2 or an explicitly selected serial console. A QEMU monitor
`info registers` and `info pic` snapshot taken while frozen can distinguish a
CPU fault, an interrupt-disabled spin, and an idle CPU waiting for an interrupt.

## Completed fixes and verification

The stability work and tasks 362–366 were implemented and tested separately:

| Change | Commit |
| --- | --- |
| Full boot test scratch space and repeatable stress acceptance | `d5eda29` |
| V7 scheduler CPU accounting and priority decay (362) | `027a594` |
| Atomic subsecond ftime (363) | `e724670` |
| IRQ-serialized kernel callouts (364) | `77c2956` |
| Sleep lost-wakeup fix and V7 alarm compatibility (365) | `59c55ec` |
| Invalid/ignored syscall compatibility (366) | `11d297a` |

Focused tests and the combined regression passed: host units and root-image
checks; ftime/sleep/syscall-policy fixtures; hostname/login/su; 1,272 shell
command checks across RAM and IDE roots; 100 two-session stability rounds per
memory size; full boot acceptance at 16/128 MiB; VGA keyboard/display and exclusive
VGA/serial routing; installed nice/sleep; and offline upgrade with two cold boots
at each memory size. Inventory, kernel coverage and public-header checks passed.
The original general freeze was not reproduced, so this is not a claim that all
possible VGA/serial freezes have been resolved.

The production `build/nexix.iso` and `build/nexix-serial.iso` were rebuilt.
To apply libc fixes to an existing installation, shut down the VM and run
`make upgrade-disk`, then boot the rebuilt ISO. Programs are statically linked;
a kernel/ISO replacement alone does not update existing disk binaries. The
offline upgrade requires a new backup path and refuses to overwrite an existing
backup; see [disk upgrades](disk-upgrades.md). Acceptance tests use disposable
images and leave the working disk unchanged.
