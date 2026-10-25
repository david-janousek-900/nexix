# Native ports of V7 mathematics and language tools (tasks 208–218).
build/v7-dc.o: user/v7/cmd/dc.c user/dc/dc.c user/dc/dc.h user/v7/compat.h | build/.dir
	$(CC) $(V7_CFLAGS) -fsigned-char -fno-builtin -include user/v7/compat.h -c $< -o $@
.PHONY: test-dc test-dc-unit
test-dc-unit:
	$(PYTHON) tests/dc.py
test-dc: test-dc-unit multi-iso rootfs
	$(PYTHON) tools/dc_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/dc_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-dc

# Task 209: V7 grammar, native-width semantic values and separately pinned lib.b.
build/bc/bc.c: user/bc/bc.y tools/generate_bc.py build/host-nx-yacc rootfs/usr/lib/yaccpar | build/.dir
	$(PYTHON) tools/generate_bc.py
build/v7-bc.o: build/bc/bc.c user/v7/compat.h | build/.dir
	$(CC) $(V7_CFLAGS) -include user/v7/compat.h -c $< -o $@
build/bc-lib.b: user/v7/bc/lib.b reference/supplemental/v6/lib.b | build/.dir
	cmp $< reference/supplemental/v6/lib.b
	cp $< $@
.PHONY: test-bc-unit test-bc
test-bc-unit: build/bc/bc.c
	$(PYTHON) tests/bc.py
test-bc: test-bc-unit multi-iso rootfs
	$(PYTHON) tools/bc_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/bc_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-bc

.PHONY: test-factor-unit test-factor
test-factor-unit:
	$(PYTHON) tests/factor.py
test-factor: test-factor-unit multi-iso rootfs
	$(PYTHON) tools/factor_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/factor_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-factor

.PHONY: test-primes-unit test-primes
test-primes-unit:
	$(PYTHON) tests/primes.py
test-primes: test-primes-unit multi-iso rootfs
	$(PYTHON) tools/primes_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/primes_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-primes
build/primes-prefix.o: tests/primes-prefix.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/primes-prefix.elf: build/crt0.o build/primes-prefix.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
test-primes: build/primes-prefix.elf
