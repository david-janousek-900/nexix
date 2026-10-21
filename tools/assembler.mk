# Native assembler components; the first-pass fixture is private until ELF output.
build/as-parse.o: user/as/parse.c user/as/as.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/as -c $< -o $@
build/as-parse-test.o: tests/as-parse.c user/as/as.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/as -c $< -o $@
build/as-parse-test.elf: build/crt0.o build/as-parse.o build/as-parse-test.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/as-parse.d build/as-parse-test.d
.PHONY: test-as-parse-unit test-as-parse
test-as-parse-unit:
	$(PYTHON) tests/assembler.py
test-as-parse: test-as-parse-unit build/as-parse-test.elf multi-iso rootfs
	$(PYTHON) tools/assembler_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/assembler_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-as-parse
test-unit: test-as-parse-unit
