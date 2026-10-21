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

build/as-encode.o: user/as/encode.c user/as/encode.h user/as/as.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/as -c $< -o $@
build/as-encode-test.o: tests/as-encode.c user/as/encode.h user/as/as.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/as -c $< -o $@
build/as-encode-test.elf: build/crt0.o build/as-parse.o build/as-encode.o build/as-encode-test.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
build/as-encoded-code.o: build/fixtures-as/encoded.S | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/as-code-runtime.o: tests/as-code-runtime.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/as-code-runtime.elf: build/crt0.o build/as-code-runtime.o build/as-encoded-code.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/as-encode.d build/as-encode-test.d build/as-code-runtime.d
.PHONY: test-as-encode-unit test-as-encode
test-as-encode-unit:
	$(PYTHON) tests/as_encoding.py
test-as-encode: test-as-encode-unit build/as-encode-test.elf build/as-code-runtime.elf multi-iso rootfs
	$(PYTHON) tools/as_encoding_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/as_encoding_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-as-encode
test-unit: test-as-encode-unit

build/as-object.o: user/as/object.c user/as/as.h user/as/encode.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/as -c $< -o $@
build/as-object-test.o: tests/as-object.c user/as/as.h user/toolchain/archive.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/as -Iuser/toolchain -c $< -o $@
build/as-object-test.elf: build/crt0.o build/as-object-test.o build/as-object.o build/as-parse.o build/as-encode.o build/toolchain-archive.o build/toolchain-object.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
build/as-object-runtime.o: build/fixtures-as/object-main.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/as-object-runtime.elf: build/crt0.o build/as-object-runtime.o build/fixtures-as/object.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/as-object.d build/as-object-test.d build/as-object-runtime.d
.PHONY: test-as-object-unit test-as-object
test-as-object-unit:
	$(PYTHON) tests/as_objects.py
test-as-object: test-as-object-unit build/as-object-test.elf build/as-object-runtime.elf multi-iso rootfs
	$(PYTHON) tools/as_objects_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/as_objects_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-as-object
test-unit: test-as-object-unit

build/v7-as.o: user/v7/cmd/as.c user/as/as.h user/toolchain/archive.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/as -Iuser/toolchain -c $< -o $@
build/v7-as2.o: user/v7/cmd/as.c user/as/as.h user/toolchain/archive.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/as -Iuser/toolchain -DAS_WORKER -c $< -o $@
build/v7-as.elf: build/crt0.o build/v7-as.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
build/v7-as2.elf: build/crt0.o build/v7-as2.o build/as-parse.o build/as-encode.o build/as-object.o build/toolchain-archive.o build/toolchain-object.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/v7-as.d build/v7-as2.d
.PHONY: test-as-unit test-as
test-as-unit:
	$(PYTHON) tests/as_driver.py
test-as: test-as-unit build/as-object-runtime.elf multi-iso rootfs
	$(PYTHON) tools/as_driver_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/as_driver_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-as
test-unit: test-as-unit
