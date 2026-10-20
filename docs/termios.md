# Native editor terminal adapter (task 378)

Traditional Vi 050325 uses termios. Nexix supplies a checked subset over its V7
line discipline; existing sgtty requests and defaults keep their historical
behavior. The native GETA/SETA records transfer settings and special characters
together. SETA validates the complete record before draining or changing state;
invalid settings and interrupted drains leave the settings unchanged.

The adapter supplies tcgetattr/tcsetattr, speed accessors, canonical/cbreak input,
echo, signals, special characters, tab modes, parity and independent input/output
CR mapping. TCSADRAIN waits for output; TCSAFLUSH also discards queued input.
The existing V7 discipline also flushes input when switching raw/cbreak/LCASE
modes. Noncanonical input supports VMIN=1 and VTIME=0 or 1: a read returns when
its first byte arrives, so an interbyte timer does not delay that read. Other
MIN/TIME combinations and unknown flags fail with EINVAL. Native serial speeds
must be supported by the selected controller, including its existing equal-speed
restriction. This is an editor adapter, not a complete POSIX terminal API.

TIOCGWINSZ returns fixed physical geometry: VGA 25x80 and serial 24x80, including
/dev/console aliases. Resizing and window-change signals are not supplied.
Login and single-user init select TERM=nexix on VGA and TERM=ansi on serial.
The system profile preserves explicit TERM settings. The terminal database and
public termios/termcap headers are installed in the full root.

`make test-termios` checks production line discipline transitions and atomic
validation plus native geometry, rejected modes, signals and restoration at
16/128 MiB. `make test-vga` checks TERM and real VGA/PS2 login with independent
serial sessions at both memory sizes. All guest disks are disposable.
