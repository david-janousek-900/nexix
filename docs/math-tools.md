# V7 mathematics and language tools

## dc (task 208)

The installed `/bin/dc` retains the V7 decimal base-100 arithmetic engine,
stack/register/array language, macros, scales, input/output bases, square roots,
file-to-stdin input and shell escapes. Active sources are in `user/dc`; the V7
reference remains unchanged. It uses native libc and does not depend on libm
or a host arithmetic library.

The native port uses signed digit bytes and full-width pointer slots. Allocation
keeps live storage until realloc succeeds; copies never read beyond the source
block. The original symbol-table sentinel overrun and stack/nesting bounds are
corrected. Integer logarithms avoid signed shifts and 32-bit long assumptions.
Shell command text is bounded and missing string terminators report failure.
Historic recoverable calculator errors retain their printed diagnostics.

`make test-dc` runs the retained engine under ASan/UBSan against 180 independently
computed integer operations and Decimal square-root fixtures, then executes the
installed command on disposable 16/128 MiB guests. Native fixtures cover decimal
fractions, exponentiation, bases, macros, register stacks, arrays through index
2047, input-file/stdin sequencing and failure paths. Existing working disks are
not test targets. Lines retain the historical 70-column continuation format.
