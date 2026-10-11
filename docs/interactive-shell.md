# Interactive shell acceptance (step 19)

`make TOOLCHAIN_PREFIX= test-shell` boots the normal production ISO with
16 MiB and 128 MiB RAM, drives COM1 through QEMU's serial chardev and runs
271 command checks per session. It uses no kernel test gates or input injection.
Transcripts are `build/interactive-16M.log` and `build/interactive-128M.log`.
The target is included in the full regression suite.

Coverage includes:

- CR/Enter, Backspace, Delete and Ctrl-U line editing.
- Quoting, exported variables, command substitution, loops, conditionals,
  subshells, exit status and recovery from a missing command.
- Pipes, append/input/output redirection, copying, hard links, working
  directories, relative/absolute globbing and full 14-byte directory names.
- Sourced scripts and interactive here documents with variable substitution.
- Background execution, wait and background cat reading EOF from `/dev/null`.
- Ctrl-C at the prompt and in a running cat; repeated interrupts; Ctrl-\
  terminating a foreground child without terminating init; pipe cancellation.
- A caught SIGINT trap, trap removal, empty and nonempty Ctrl-D input records.
- A 5120-byte variable expansion and 200 consecutive fork/exec/wait commands.

After interrupts and at session end, the shell remains PID 2 and accepts
commands; temporary files have been removed. The complementary `test-boot`
checks verify PID 1 and shell restart after console EOF. Component regression
tests still check process, physical-page, inode, file and TTY reference cleanup.
Interactive sessions do not expose kernel allocator counters.

The acceptance work fixes three porting gaps. Current-directory expansion now
looks up `.` rather than an empty pathname, while emitted names remain relative.
Init starts with process group zero, and V7-style terminal open gives its console
child the terminal group; terminal interrupts and quits no longer target init.
The root includes `/dev/null`, character device 0x0300 with mode 0666. Reads return
EOF and writes report the discarded byte count after normal user-buffer checks.
The device acquires no TTY reference and allocates no data blocks. Stat reports
size zero; lseek maintains a logical offset without affecting its I/O.

To test manually, run `make TOOLCHAIN_PREFIX= run` and try:

```sh
echo hello | cat
V='two words'; echo "$V"
echo first > /tmp/a; echo second >> /tmp/a; cat /tmp/a
cd /tmp; echo a*; rm a
cat
```

While cat runs, enter a line, then Ctrl-C or Ctrl-\; the shell prompt returns.
Ctrl-U erases the line being entered. Ctrl-D on an empty shell input line ends
the single-user session and init starts another shell. Ctrl-A, release, then X
exits QEMU. See [booting.md](booting.md) for startup and storage behavior.

This remains V7 shell behavior: there are no modern job-control commands,
readline/history or bash extensions. Cooked input lines are limited to 256
bytes; expanded arguments also obey the native exec limits. Persistent IDE
storage is implemented in step 20; see [storage.md](storage.md). Networking
belongs to version 0.2.0.
