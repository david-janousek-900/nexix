# Manual pager

The native `more` command reads a named file or standard input. On a terminal
with termcap cursor motion/clear capabilities it provides:

| Key | Action |
|---|---|
| Space, f, Ctrl-F, Page Down | Next page |
| b, Ctrl-B, Page Up | Previous page |
| j, Enter, Down / k, Up | Next / previous line |
| d, Ctrl-D / u, Ctrl-U | Forward / backward half-page |
| g, Home / G, End | Beginning / last page |
| /text / ?text | Literal, case-sensitive forward / backward search |
| n / N | Repeat / reverse the previous search |
| Escape during search | Cancel search entry |
| q / h | Quit / show key help |

Terminal dimensions come from the output terminal. Navigation reads that same
terminal, including when stdin carries a formatter pipe. Display normalization
handles nroff overstrikes, tabs and long lines, and prevents document escape
bytes from controlling the screen. Redirected output is copied byte-for-byte;
unsupported or unspecified terminals get plain output. Input and normalized
text each have a 4 MiB limit; allocation and input errors are checked.

Terminal modes are restored on exit and catchable signals. Interrupt/quit keys
are interpreted by the pager while it runs, including the single-user console.
The native target has fixed VGA/serial geometries; resize and Unicode display
are outside this pager's current scope.

`/usr/libexec/manfmt` runs a formatter and terminal pager, waits for both, and
preserves failures. Non-terminal output executes the formatter directly.
`MANPAGER` overrides `PAGER`, with `/bin/more` as the default. Pager arguments
are split on spaces/tabs; shell quoting and metacharacters are not evaluated.
A pager failure takes precedence over a formatter failure when both fail.

`make test-pager` checks actual VGA cells and an independent serial screen model
at 16/128 MiB, movement/search, arrow/page keys, redirection, document controls,
interrupt restoration and formatter/pager errors.
