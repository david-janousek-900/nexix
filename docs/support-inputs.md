# Missing support-input resolution (task 88)

The pinned V7 archive does not contain bc's lib.b or lex's nrform/ebcform.
Their disposition is now explicit in [support-inputs.json](support-inputs.json):
all three features retain source inputs for later consumer ports. No optional
mode is waived and no additional bytes are inserted into `reference/v7`.
Recovered originals and licenses are separately hash-pinned under
`reference/supplemental`; active adaptations live under `user/v7`.

| Input | Disposition | Active input | Remaining consumer acceptance |
| --- | --- | --- | --- |
| bc lib.b | Historical V6 source carried in the V7 source-history snapshot; six math functions, unmodified | `user/v7/bc/lib.b` | Task 209: native bc/dc compilation, -l installation and numerical fixtures |
| lex nrform | Later illumos source, separately CDDL-licensed; adapted to V7 generated COMMON layout | `user/v7/lex/nrform` | Tasks 171–173/216 and native Fortran: generate/translate/compile/run Ratfor scanners and library helpers |
| lex ebcform | Exact original file remains unrecovered; explicit native equivalent derived from supplied V7 ncform | `user/v7/lex/ebcform` | Tasks 171–173: generated extended-table scanners, compressed transitions and end-to-end acceptance |

The recovered bc file defines e, s, c, l, a and j and sets scale 20. Its origin
is stated as V6 source lineage, not asserted to be a recovered V7 archive member.
The Ratfor source's CDDL header is retained in both original and adapted files;
its accompanying OPENSOLARIS.LICENSE is preserved. The adapted COMMON fields
match V7 header.c's yyfnd/yymorf/yyprev/yybgin/yylsp/yylsta ordering, correct the
later skeleton's ymorf typo, and use V7's generated YYTOPVAL constant instead of
inserting an incompatible yytop field. This supplies the missing template;
it does not claim a working native Ratfor/Fortran toolchain yet.

The file named ebcform is selected by V7 when an extended `%t` character table
has ZCH>NCH. Thus this is broader than a command-line EBCDIC output switch.
The native equivalent keeps that extended-byte support on x86_64. It adapts the
supplied C scanner with unsigned-byte promotion and uintptr_t pointer comparisons,
and fixes a failed-transition/EOF loop in its non-YYOPTIM path. It does not
claim to recover the original ebcform text or emulate an EBCDIC-host C compiler.
The native generator port still must test arbitrary translated character tables
and all generated support routines; those tests belong to tasks 171–173.

`make test-support-inputs` verifies source dispositions, the six bc function
inputs and balanced definitions, Ratfor COMMON compatibility, and executes the
extended C driver for every non-NUL byte 1–255. It tests both char signedness
choices, with/without YYOPTIM, under undefined-behavior sanitizer and bounded
input reads. NUL remains the historical scanner end sentinel. These runtime
fixtures use a concrete scanner table and EOF, not a prebuilt V7 binary.
`make test-supplemental-inputs` verifies every recovered file against its SHA-256,
immutable upstream URL and associated license. All checks run in test-unit.
Source availability is complete for task 88; installation and full language/
generator semantics remain explicitly assigned to their consumer tasks.
