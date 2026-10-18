# Native kernel configuration and build (task 78)

`config/kernel.json` is the native counterpart of the V7 `usr/sys/conf`
configuration input. `tools/mkconf.py` validates it and generates
`build/native-config.mk`. Make includes this file and uses its driver object
list in the actual kernel link. The generated file is rebuilt when the input
or generator changes and is recreated after `make clean`. Driver source files
and normal object prerequisites retain dependency-file tracking.

The supported profile is x86_64 with COM1, COM2, IDE, RTC and x87/SSE2, IDE root
with the fixed bootstrap fallback, and resident memory. All five drivers are
required by this profile: silently dropping a driver would leave configuration
claims inconsistent with kernel callers and the distribution. Unknown keys,
architectures, drivers, duplicates, omitted drivers and unsupported root/swap
models fail before linking. Driver order is canonical and output is deterministic.
Alternative hardware profiles require implementation and acceptance tests before
the generator will accept them.

| Reference configuration/build responsibility | Native equivalent |
| --- | --- |
| `conf/mkconf.c` and historical `conf/mkconf` executable | Python generator executed on the host; historical PDP-11 executable is never run |
| Generated `conf/c.c`, device switch tables | Validated native driver objects plus existing native dispatch in files/storage/tape/TTY |
| Generated `conf/l.s`, `mch*.s` | Native boot, descriptor tables and interrupt stubs/handlers |
| hp/rp/rk root and ht/tm configuration samples | Native IDE root; historical controller interfaces map to documented image backends rather than PDP-11 MMIO |
| swplo/nswap and swap device | Resident process memory; no disk-backed swap requirement |
| `conf/makefile`, sys/dev mklib and LIB1/LIB2 | Makefile compiles native C/assembly and directly links ELF64, replacing PDP-11 object archives |

Run `make test-mkconf` for valid/reordered input, all source paths, malformed
configuration and unsupported hardware checks. `make build` consumes generated
configuration; `make test-boot test-multiuser-acceptance` verifies the resulting
kernel in RAM fallback and full IDE multi-user boots at 16/128 MiB. The fixed
bootstrap and existing working disk are retained by the normal build.
