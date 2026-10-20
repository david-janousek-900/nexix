# Shared V7 formatter core; architecture adaptations live in user/troff.
ROFF_NAMES = n1 n2 n3 n4 n5 n6 n7 n8 n9 n10 ni nii ntab hytab suftab
ROFF_OBJECTS = $(addprefix build/nroff-,$(addsuffix .o,$(ROFF_NAMES)))
ROFF_CFLAGS = $(V7_CFLAGS) -fno-toplevel-reorder -Iuser/troff -include user/troff/native.h -DNROFF
build/nroff-%.o: user/troff/%.c $(wildcard user/troff/*.h) | build/.dir
	$(CC) $(ROFF_CFLAGS) -c $< -o $@
build/v7-nroff.elf: build/crt0.o $(ROFF_OBJECTS) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o $(ROFF_OBJECTS) build/libc.a -o $@
-include $(ROFF_OBJECTS:.o=.d)
.PHONY: test-formatter-core
test-formatter-core: build/v7-nroff.elf build/nexix-multi.iso build/roff-data/stamp
	$(PYTHON) tools/formatter_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/formatter_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-formatter-core
.PHONY: test-formatter-macros
test-formatter-macros: build/v7-nroff.elf build/nexix-multi.iso
	$(PYTHON) tests/formatter_macros.py
	$(PYTHON) tools/formatter_macros_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/formatter_macros_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-formatter-macros
