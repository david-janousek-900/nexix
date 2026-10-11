# Time, timezone, date and touch (step 24)

The kernel clock and RTC use UTC epoch seconds. Filesystem timestamps retain
V7's existing second precision and 32-bit disk encoding. Timezone conversion
happens in each program's libc and never changes stored timestamps.

## Configuration

The native profile defaults to `TZ=GMT0`. Change `rootfs/profile` before creating
a new root, or set and export TZ inside the shell:

```sh
TZ=CET-1CEST,M3.5.0,M10.5.0/3; export TZ
date
date -u
```

This selects CET/CEST with the last Sunday of March at 02:00 standard time and
the last Sunday of October at 03:00 daylight time. The `TZ` sign gives the
amount added to local time to obtain UTC: `CET-1` is UTC+1 and `EST5` is UTC-5.
Programs inherit exported TZ; libc rereads it on each local-time conversion.
`ls -l` uses the same timezone through ctime.

Supported grammar is `STDoffset[DST[,Mmonth.week.weekday[/time],M...[/time]]]`.
Names contain 3–15 ASCII letters; offsets are signed hours with optional
`:minutes`, at most 24 hours. DST adds one hour. Months are 1–12, weeks 1–5
(5 means the last matching weekday), and weekdays 0–6 (Sunday is 0).
Transition times are nonnegative hours with optional `:minutes`, default 02:00.
Northern and southern seasonal intervals are supported. Examples:

- `GMT0`: UTC/GMT without DST.
- `IST-5:30`: UTC+05:30 without DST.
- `EST5EDT`: V7's historical US DST rules, including its 1974/1975 exceptions.
- `EST5EDT,M3.2.0,M11.1.0`: explicit recurring March/November rules.

Unset, empty or malformed TZ falls back to GMT. IANA names such as
`Europe/Prague`, zoneinfo files, Julian/day-number rules, quoted names, second
precision offsets and explicit non-hour DST offsets are not implemented. Use
an explicit rule string for contemporary local DST instead of the historical
V7 defaults. This is a documented TZ subset, not a timezone database.

## Native libc

`gmtime`, `localtime`, `asctime`, `ctime`, `timezone`, `dysize`, `tzset` and `ftime`
are available. The V7 `struct tm` uses nine integer fields and `tm_year` counts
years since 1900. Conversion uses the Gregorian calendar, including the
non-leap year 2100; negative epochs are supported down to year 1. Upper bound
is year 9999. Invalid arguments return NULL/EINVAL where applicable. Results
use shared static storage, as in V7.

`ftime` returns current whole seconds, minutes west of UTC and a flag indicating
whether the selected zone allows DST. `millitm` is zero because the current
native clock interface has second precision. The native timeb contains a
64-bit time field; it is not a PDP-11 binary ABI. Kernel `stime` remains root
only and does not write the RTC. Timers and alarms remain monotonic.

## Retained V7 commands

`date` prints local time and a timezone name. `date -u` prints GMT. Root may set
time with `date [-u] YYMMDDHHMM[.SS]`; omitted year, month and day use current
values, and 24:00 rolls into the following day. Examples:

```sh
date -u 2402291234.56
date 0708
```

Two-digit years use the native window 1970–2069 (70–99 map to 1970–1999;
00–69 map to 2000–2069). Omitted years retain the current full year. This
adaptation is necessary for present-day RTC dates; the original assumes the
1900s. Gregorian leap calculations use the matching year representation.
With `-u`, omitted fields come from GMT. Other parsing and local DST adjustment
retain the reference behavior, including calendar-day normalization and its
ambiguity around skipped/repeated local hours. Explicit UTC setting avoids
that ambiguity. Successful settings append the V7 `|` and `{` old/new records
to `/usr/adm/wtmp` using the existing native 24-byte utmp record layout.

`touch [-c] file ...` retains V7's implementation: read and rewrite the first
byte of a nonempty file, or create/truncate an empty file. This preserves
nonempty contents and relies on kernel timestamp handling. `-c` prevents
creation of subsequent missing operands and retains the original diagnostic
and exit behavior. It does not add modern `-a`, `-m`, `-t`, or reference-file
options. Touching an empty existing file updates modification/change time;
its access time is unchanged, matching the original creat path.

Original distribution files remain unchanged in `reference/v7`. Native
command copies live in `user/v7/cmd`. `make TOOLCHAIN_PREFIX= test-time` checks
the conversion rules, real command behavior and accounting in QEMU.

Existing persistent roots do not receive new executables automatically. Run
`make TOOLCHAIN_PREFIX= DISK_IMAGE=build/nexix-step24.img run` to create a
separate root containing date/touch and the new default profile. Existing
files in `build/nexix.img` are preserved; full upgrade handling is step 357.
