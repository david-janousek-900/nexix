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
build/v7-size.o: user/v7/cmd/size.c user/toolchain/object.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/toolchain -c $< -o $@
build/v7-size.elf: build/crt0.o build/v7-size.o build/toolchain-object.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/v7-size.d
.PHONY: test-size-unit test-size
test-size-unit:
	$(PYTHON) tests/object_tools.py size
test-size: test-size-unit multi-iso rootfs
	$(PYTHON) tools/object_tools_smoke.py $(QEMU) build/nexix-multi.iso 16M size
	$(PYTHON) tools/object_tools_smoke.py $(QEMU) build/nexix-multi.iso 128M size
test: test-size
test-unit: test-size-unit
