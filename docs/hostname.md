# Hostname and login prompts

`/etc/hostname` stores one complete hostname, including its domain, followed by
a newline. Fresh images use `nexix.localdomain`. Init reads this file before
starting the first recovery shell or any getty, so the running kernel's node
name and `uname -n` agree with it. Editing the file takes effect on the next
boot; `hostname -F /etc/hostname` loads it immediately.

```sh
hostname
hostname workstation.example.org
hostname -f
hostname -s
hostname -d
```

`hostname` and `-f` print the full name, `-s` prints the first label, and `-d`
prints the remaining domain. A name argument requires root and saves the change.

`hostname -F file` is a root-only runtime loader: it reads a file and changes
the kernel name without rewriting `/etc/hostname`. `hostname name` persists
before publishing the new kernel state; a failed configuration write leaves
the running name unchanged. The configuration is written in place because
Nexix has V7 link/unlink operations and no atomic rename syscall. An I/O failure
can leave that file incomplete: correct it before rebooting. Fresh files are
root-owned mode 0644. Existing file permissions are retained.

Names use ASCII letters/digits and internal hyphens, with dots separating
nonempty labels. A label is at most 63 bytes and the complete name at most 64,
matching the existing uname record. Whitespace, embedded NULs, empty labels,
leading/trailing hyphens and trailing dots are rejected. File loading accepts
one line with optional LF/CRLF and rejects extra text. Missing or invalid boot
configuration leaves the kernel fallback `nexix`; it does not block recovery.
No DNS lookup, NIS domain configuration or network support is implied.

`gethostname(char *, size_t)` and `sethostname(const char *, size_t)` are public
libc interfaces in `<unistd.h>`. Getters include a NUL terminator; a buffer too
small returns -1/ERANGE without changing it. Setters use native syscall 75,
require effective UID 0 and validate/copy the whole name before changing shared
state. Invalid input and EFAULT/EPERM leave the previous name unchanged.

## Prompts

Login shells read `/etc/profile` and then their home `.profile`; home settings
can override the system defaults. Fresh system and home profiles set:

```sh
PS1='\u@\H \$ '
export PS1
```

The native Bourne shell renders `\u` as the effective user's account name,
`\H` as the full current hostname, `\h` as its first label and `\$` as `#` for
effective UID 0 or `$` otherwise. `\\` displays a backslash; other escapes
remain literal. Hostname changes are visible on the next prompt, including
already logged-in terminals. `su` preserves its historical HOME/cwd behavior
while displaying the new user's name and privilege marker.

Account lookup happens before the V7 shell initializes its private sbrk heap;
prompt rendering allocates no memory and performs no shell expansion. Normal
prompts are written together so input echo cannot split their fields. The
fresh `/root/.profile`, `/.profile` and regular-user profiles also export this
prompt. Init supplies HOME=/root to recovery shells; the shared `/.profile`
does not replace a regular user's HOME. Upgrades preserve existing home profiles; the newly installed global
`/etc/profile` supplies the prompt unless a home profile explicitly overrides
it. Existing `/etc/hostname` files are preserved by offline upgrades.

## Verification

`make test-hostname` exercises kernel validation/permissions and mutation
rollback, then boots the full distribution at 16 and 128 MiB. Three cold boots
verify saved names, manually edited domain names, inherited profiles, root and
Alice prompts on two terminals, live updates, nested su, API buffer/permission
errors and configuration-write failure. The ordinary uname, login, interactive
shell and multi-user regressions also cover the integration.
