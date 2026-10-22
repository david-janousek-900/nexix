# lint1 over the native PCC front end, lint2 and the installed driver (tasks 261-262).
LINT1_OBJECTS = $(addprefix build/pcc-,$(addsuffix .o,$(filter-out scan local,$(PCC_FRONT_NAMES)))) build/pcc-cgram.o build/lint-scan.o build/lint-local.o build/lint-lint.o
build/lint-scan.o: user/pcc/scan.c user/pcc/mfile1 user/pcc/manifest user/pcc/macdefs user/pcc/prototypes.h | build/.dir
	$(CC) $(PCC_CFLAGS) -DLINT -c $< -o $@
build/lint-local.o: user/pcc/local.c user/pcc/mfile1 user/pcc/manifest user/pcc/macdefs user/pcc/prototypes.h | build/.dir
	$(CC) $(PCC_CFLAGS) -Dclocal=pcc_native_clocal -Dandable=pcc_native_andable -Dexname=pcc_native_exname -c $< -o $@
build/lint-lint.o: user/lint/lint.c user/lint/lmanifest user/pcc/mfile1 user/pcc/manifest user/pcc/macdefs user/pcc/prototypes.h | build/.dir
	$(CC) $(PCC_CFLAGS) -Iuser/lint -c $< -o $@
build/lint-lpass2.o: user/lint/lpass2.c user/lint/lmanifest user/mip/manifest.h | build/.dir
	$(CC) $(V7_CFLAGS) -Iuser/lint -c $< -o $@
build/v7-lint1.elf: build/crt0.o $(LINT1_OBJECTS) $(MIP_SHARED) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
build/v7-lint2.elf: build/crt0.o build/lint-lpass2.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/lint-scan.d build/lint-local.d build/lint-lint.d build/lint-lpass2.d

.PHONY: test-lint-unit test-lint
test-lint-unit: build/gen-pcc/cgram.c
	$(PYTHON) tests/lint.py
test-lint: test-lint-unit build/v7-lint1.elf build/v7-lint2.elf multi-iso rootfs
	$(PYTHON) tools/lint_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/lint_smoke.py $(QEMU) build/nexix-multi.iso 128M
test-unit: test-lint-unit
test: test-lint
