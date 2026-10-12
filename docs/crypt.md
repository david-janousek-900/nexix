# V7 DES and password primitives (step 46)

`libc/crypt.c` is source-adapted from
`reference/v7/usr/src/libc/gen/crypt.c`. All ten permutation, shift, expansion
and S-box tables and the DES round/password algorithms are retained. Historical
implicit declarations and initializer syntax are converted to native C11.
The original source and crypt(3) manual remain unchanged.

The public interfaces are in `<crypt.h>` and also visible through `<unistd.h>`:

```c
char *crypt(const char *password, const char *salt);
void setkey(const char key[64]);
void encrypt(char block[64], int decrypt);
```

crypt uses the first eight password bytes, ignoring their high bits. Its two
salt characters use the V7 alphabet `./0-9A-Za-z` to select one of 4096 expansion
permutations. It encrypts the zero block 25 times and returns the historical
13-character result: two salt characters followed by eleven encoded characters.
A full previous hash can be passed as the salt, as login/password consumers do;
only its first two characters are used. Results share static storage and are
replaced by the next crypt call.

The reference's one-character salt extension is retained: its NUL second byte
participates in the original salt transformation, then the first character is
copied into the second output position. This is not equivalent to hashing with
two copies of that salt character. Normal callers use two valid salt characters;
other nonspecified salt characters are not given new encoding semantics.
NULL arguments and an empty salt return NULL/EINVAL rather than reading outside
the supplied string. The password reader stops after eight bytes without reading
a ninth byte, which also supports makekey's fixed eight-byte input contract.

setkey/encrypt use 64 separate bytes with numerical values 0 or 1, not an
eight-byte packed DES key/block. Key bits at positions 8,16,...64 are parity
bits and are ignored. encrypt modifies its 64-byte argument in place; zero
selects encryption and any nonzero flag selects decryption. A key must first be
established with setkey. The routines reject NULL/non-bit arrays with EINVAL,
leaving the previous key or input block unchanged. Array lengths remain the
caller's responsibility.

## Explicit native storage/state adaptations

The reference indexes `L` through 63 even though it declares separate L[32]
and R[32] arrays. Native code uses one 64-byte array with explicit half views,
so optimization/linker ordering cannot break that assumed adjacency.

The supplied reference initializes the E expansion table only inside crypt.
The native setkey initializes ordinary DES expansion too, allowing the documented
setkey/encrypt interface to work in a fresh process and after password hashing.
crypt still applies its salt swaps afterward and leaves its shared key/expansion
state, as the original does. Calling encrypt without an established key returns
with EINVAL instead of indexing an uninitialized expansion table.

The interfaces are V7-style shared-state, non-reentrant primitives. Password
files, default accounts, getpass, login/su/newgrp/passwd and makekey are not
installed or ported by this step. They retain steps 47, 99, 102–104 and 246.
This does not add a new password hash format or change account credentials.

## Verification

```sh
make test-crypt test-unit test-inventory
```

The same fixed-vector tests run on the host and in ring 3 at 16/128 MiB. They
verify key `133457799BBCDFF1`, plaintext `0123456789ABCDEF` and expected DES
ciphertext `85E813540F0AB405`, reverse decryption, ignored parity bits and
untouched block guards. Password fixtures include:

| Password | Salt | Expected result |
|---|---|---|
| empty | `..` | `..X8NBuQ4l6uQ` |
| `password` | `ab` | `abJnggxhB/yWI` |
| `hello` | `HX` | `HXWGxfat5w9NQ` |
| `12345678` | `zz` | `zzRtj6pNdfpLE` |
| `abcdefgh` | `./` | `./GLbXuBxqD4c` |

Additional checks cover static-buffer reuse, passing a full stored hash as salt,
eight-byte truncation, high-bit handling, single-character salt behavior,
setkey after crypt, and invalid input without state corruption. A golden-source
checker compares every original DES table value with its native counterpart.

An optional differential fixture loads the independently installed host
libcrypt and compares all 4096 salts for `password`. All comparisons passed on
the development host. Host libcrypt is not linked into Nexix; its absence skips
only this optional oracle, while fixed known-answer tests remain mandatory.
Existing boot/libc/userland/filesystem/process cleanup checks run afterward.
The public declaration/export audit now finds 198 declarations and 198 exports;
remaining libc features and full consumer acceptance remain on the roadmap.
