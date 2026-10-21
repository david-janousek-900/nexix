# The resolver is private until executable production and installed acceptance.
build/ld-resolve.o: user/ld/resolve.c user/ld/ld.h user/toolchain/object.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/ld -Iuser/toolchain -c $< -o $@
build/ld-resolve-test.o: tests/ld-resolve.c user/ld/ld.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/ld -Iuser/toolchain -c $< -o $@
build/ld-resolve-test.elf: build/crt0.o build/ld-resolve.o build/ld-resolve-test.o build/toolchain-object.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/ld-resolve.d build/ld-resolve-test.d
.PHONY: test-ld-resolve-unit test-ld-resolve
test-ld-resolve-unit:
	$(PYTHON) tests/ld_resolution.py
test-ld-resolve: test-ld-resolve-unit build/ld-resolve-test.elf multi-iso rootfs
	$(PYTHON) tools/ld_resolution_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/ld_resolution_smoke.py $(QEMU) build/nexix-multi.iso 128M
test-unit: test-ld-resolve-unit
test: test-ld-resolve

build/ld-output.o: user/ld/output.c user/ld/ld.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/ld -Iuser/toolchain -c $< -o $@
build/ld-output-test.o: tests/ld-output.c user/ld/ld.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/ld -Iuser/toolchain -c $< -o $@
build/ld-output-test.elf: build/crt0.o build/ld-resolve.o build/ld-output.o build/ld-output-test.o build/toolchain-object.o build/toolchain-archive.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/ld-output.d build/ld-output-test.d
.PHONY: test-ld-output-unit test-ld-output
test-ld-output-unit: build/crt0.o build/libc.a
	$(PYTHON) tests/ld_output.py
test-ld-output: test-ld-output-unit build/ld-output-test.elf multi-iso rootfs
	$(PYTHON) tools/ld_output_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/ld_output_smoke.py $(QEMU) build/nexix-multi.iso 128M
test-unit: test-ld-output-unit
test: test-ld-output

build/ld-relocatable.o: user/ld/relocatable.c user/ld/ld.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/ld -Iuser/toolchain -c $< -o $@
build/v7-ld.o: user/v7/cmd/ld.c user/ld/ld.h user/toolchain/archive.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/ld -Iuser/toolchain -c $< -o $@
build/v7-ld.elf: build/crt0.o build/v7-ld.o build/ld-resolve.o build/ld-output.o build/ld-relocatable.o build/toolchain-object.o build/toolchain-archive.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/ld-relocatable.d build/v7-ld.d
.PHONY: test-ld-unit test-ld
test-ld-unit: test-ld-output-unit test-ld-resolve-unit
	$(PYTHON) tests/ld_driver.py
test-ld: test-ld-unit multi-iso rootfs
	$(PYTHON) tools/ld_driver_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/ld_driver_smoke.py $(QEMU) build/nexix-multi.iso 128M
test-unit: test-ld-unit
test: test-ld
