# Kernel messages and dmesg

Every kernel diagnostic line has a monotonic uptime prefix, for example
`[    0.120000] RTC UTC epoch=0x0000000065e079f0`. Time starts at zero and has
10 ms resolution from the 100 Hz PIT; setting the wall clock does not change it.
Fragments such as hex values share their line's timestamp. Diagnostic prefixes
`Nexix:` and `NEXIX_` are replaced by readable message text. User-program output,
login prompts and passwords are not kernel log entries and are not timestamped.

The kernel retains the latest 16 KiB of text in a ring independent of console
selection. Snapshots are chronological, use LF line endings, omit a partially
overwritten oldest line and do not clear the buffer. Boot messages remain
available after login. Panic messages use the same formatter and buffer.

As root, run `dmesg` (`/bin/dmesg`, also `/etc/dmesg`) to print a snapshot.
`dmesg -` prints messages since its previous successful invocation and saves its
snapshot in `/usr/adm/msgbuf` (root-owned mode 0600). The first invocation prints
all retained messages; a repeated invocation without new messages is silent.
If no saved suffix overlaps the current buffer, it prints `...` and the retained
messages. Errors return 1; invalid operands return 2. Historical memory/namelist
operands are deliberately rejected: native Nexix uses a checked syscall instead
of exposing kernel addresses through `/dev/mem` or requiring `/unix` symbols.

`NX_SYS_KLOG` is a root-only, non-destructive snapshot interface. NULL/0 queries
its current size; too-small buffers fail with ERANGE without writes, invalid
addresses with EFAULT. Permission failures return EPERM. Reading does not log
itself or capture terminal traffic. The 16 KiB bound is in `nexix/klog.h`.

`make test-klog-unit` checks timestamps, fragmented writes, ring wrap, routing,
authorization and copy boundaries. `make test-dmesg` exercises real guest CLI
reads, incremental mode, argument errors and denied access at 16/128 MiB.
`make test-console` covers kernel messages and panic on VGA and serial, plus
root's `/root` login and native Backspace editing.

Fresh installations give root a private mode-0700 `/root` and `/root/.profile`.
The recovery startup profile remains at `/.profile` as well. Existing persistent
disks are preserved by builds. The [offline upgrader](disk-upgrades.md) installs
new programs and missing directories while retaining existing accounts and TZ;
review preserved configuration when migrating an older root home. A fresh root
can instead be created with an unused `DISK_IMAGE`.
