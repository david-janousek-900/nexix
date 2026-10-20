# Portable data is regenerated from source for the native formatter.
ROFF_TABLE_INPUTS = $(wildcard reference/v7/usr/src/cmd/troff/term/* reference/v7/usr/src/cmd/troff/font/*)
build/roff-data/stamp: tools/formatter_tables.py $(ROFF_TABLE_INPUTS) | build/.dir
	$(PYTHON) tools/formatter_tables.py --cc $(HOST_CC)
build/roff-data/%: build/roff-data/stamp
	@test -f $@
.PHONY: test-formatter-tables
test-formatter-tables: build/roff-data/stamp
	$(PYTHON) tests/formatter_tables.py
test: test-formatter-tables
# Retained source helpers are also executable on the target.
build/roff-mkfont.o: user/troff/font/mkfont.c user/troff/font/mkfont1.c | build/.dir
	$(CC) $(V7_CFLAGS) -c $< -o $@
build/roff-chars.o: user/troff/font/chars.c | build/.dir
	$(CC) $(V7_CFLAGS) -c $< -o $@
build/roff-tab3.o: user/troff/tab3.c | build/.dir
	$(CC) $(V7_CFLAGS) -c $< -o $@
build/roff-mkfont.elf: build/crt0.o build/roff-mkfont.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/roff-mkfont.o build/libc.a -o $@
build/roff-chars.elf: build/crt0.o build/roff-chars.o build/roff-tab3.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/roff-chars.o build/roff-tab3.o build/libc.a -o $@
test-formatter-tables: build/roff-mkfont.elf build/roff-chars.elf
.PHONY: test-formatter-table-helpers
test-formatter-table-helpers: test-formatter-tables build/nexix-multi.iso
	$(PYTHON) tools/formatter_tables_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/formatter_tables_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-formatter-table-helpers
