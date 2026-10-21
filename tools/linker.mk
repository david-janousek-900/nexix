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
