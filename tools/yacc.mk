# Native yacc keeps the supplied LALR generator, separately from host bootstrap.
YACC_NATIVE_OBJECTS = $(addprefix build/yacc-,$(addsuffix .o,y1 y2 y3 y4))
build/yacc-%.o: user/yacc/%.c user/yacc/dextern user/yacc/files | build/.dir
	$(CC) $(V7_CFLAGS) -Wno-comment -c $< -o $@
build/v7-yacc.elf: build/crt0.o $(YACC_NATIVE_OBJECTS) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include $(YACC_NATIVE_OBJECTS:.o=.d)
.PHONY: test-yacc-unit test-yacc
test-yacc-unit:
	$(PYTHON) tests/yacc.py
test-yacc: test-yacc-unit multi-iso rootfs
	$(PYTHON) tools/yacc_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/yacc_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-yacc
test-unit: test-yacc-unit

# Private acceptance programs compiled from the exact generated C compared in guests.
build/yacc-runtime.o: build/fixtures-yacc/runtime.c | build/.dir
	$(CC) $(V7_CFLAGS) -c $< -o $@
build/yacc-recovery.o: build/fixtures-yacc/recovery.c | build/.dir
	$(CC) $(V7_CFLAGS) -c $< -o $@
build/yacc-runtime.elf: build/crt0.o build/yacc-runtime.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
build/yacc-recovery.elf: build/crt0.o build/yacc-recovery.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
# These files are emitted by the sanitizer acceptance generator, not make's
# implicit host-yacc rule for the adjacent grammar fixtures.
build/fixtures-yacc/runtime.c build/fixtures-yacc/recovery.c: ;
