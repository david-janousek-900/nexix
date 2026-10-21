# Private shared compiler consumer; no installed compiler is claimed yet.
build/mip-common.o: user/mip/common.c user/mip/mip.h user/mip/manifest.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/mip -c $< -o $@
build/mip-common-test.o: tests/mip-common.c user/mip/mip.h user/mip/manifest.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/mip -c $< -o $@
build/mip-common-test.elf: build/crt0.o build/mip-common.o build/mip-common-test.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/mip-common.d build/mip-common-test.d
.PHONY: test-mip-unit test-mip
test-mip-unit:
	$(PYTHON) tests/mip.py
test-mip: test-mip-unit build/mip-common-test.elf multi-iso rootfs
	$(PYTHON) tools/mip_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/mip_smoke.py $(QEMU) build/nexix-multi.iso 128M
test-unit: test-mip-unit
test: test-mip
