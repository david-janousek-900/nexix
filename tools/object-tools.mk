# Native ELF/archive command front ends share checked format readers.
build/toolchain-object.o: user/toolchain/object.c user/toolchain/object.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/toolchain -c $< -o $@
build/v7-nm.o: user/v7/cmd/nm.c user/toolchain/object.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/toolchain -c $< -o $@
build/v7-nm.elf: build/crt0.o build/v7-nm.o build/toolchain-object.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/toolchain-object.d build/v7-nm.d
.PHONY: test-nm-unit test-nm
test-nm-unit:
	$(PYTHON) tests/object_tools.py nm
test-nm: test-nm-unit multi-iso rootfs
	$(PYTHON) tools/object_tools_smoke.py $(QEMU) build/nexix-multi.iso 16M nm
	$(PYTHON) tools/object_tools_smoke.py $(QEMU) build/nexix-multi.iso 128M nm
test: test-nm
test-unit: test-nm-unit
