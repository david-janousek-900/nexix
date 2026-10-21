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

MIP_SHARED = build/mip-common.o build/mip-trees.o build/mip-match.o build/mip-allo.o build/mip-diagnostics.o
build/mip-%.o: user/mip/%.c user/mip/mip.h user/mip/templates.h user/mip/diagnostics.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/mip -c $< -o $@
build/mip-infra-test.o: tests/mip-infrastructure.c user/mip/mip.h user/mip/templates.h user/mip/diagnostics.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/mip -c $< -o $@
build/mip-infra-test.elf: build/crt0.o build/mip-infra-test.o $(MIP_SHARED) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include $(MIP_SHARED:.o=.d) build/mip-infra-test.d
.PHONY: test-mip-infra-unit test-mip-infra
test-mip-infra-unit:
	$(PYTHON) tests/mip_infrastructure.py
test-mip-infra: test-mip-infra-unit build/mip-infra-test.elf multi-iso rootfs
	$(PYTHON) tools/mip_infrastructure_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/mip_infrastructure_smoke.py $(QEMU) build/nexix-multi.iso 128M
test-unit: test-mip-infra-unit
test: test-mip-infra
