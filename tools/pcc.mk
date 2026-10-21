# Retained PCC front end, privately inspected until native backend/driver closure.
PCC_FRONT_NAMES = scan pftn trees optim xdefs common local numbers ir
PCC_FRONT_OBJECTS = $(addprefix build/pcc-,$(addsuffix .o,$(PCC_FRONT_NAMES)))
PCC_CFLAGS = $(V7_CFLAGS) -Iuser/pcc -Wno-comment -Wno-return-mismatch -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast
build/gen-pcc/cgram.c: user/pcc/cgram.y tools/pcc_generate.py rootfs/usr/lib/yaccpar build/host-v7-yacc
	$(PYTHON) tools/pcc_generate.py
build/pcc-cgram.o: build/gen-pcc/cgram.c user/pcc/mfile1 user/pcc/manifest user/pcc/macdefs user/pcc/prototypes.h
	$(CC) $(PCC_CFLAGS) -c $< -o $@
build/pcc-%.o: user/pcc/%.c user/pcc/mfile1 user/pcc/manifest user/pcc/macdefs user/pcc/prototypes.h | build/.dir
	$(CC) $(PCC_CFLAGS) -c $< -o $@
build/pcc-front-test.elf: build/crt0.o $(PCC_FRONT_OBJECTS) build/pcc-cgram.o build/pcc-trace.o build/mip-common.o build/mip-trees.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include $(PCC_FRONT_OBJECTS:.o=.d) build/pcc-cgram.d build/pcc-trace.d
.PHONY: test-pcc-front-unit test-pcc-front
test-pcc-front-unit: build/gen-pcc/cgram.c
	$(PYTHON) tests/pcc_front.py
test-pcc-front: test-pcc-front-unit build/pcc-front-test.elf multi-iso rootfs
	$(PYTHON) tools/pcc_front_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/pcc_front_smoke.py $(QEMU) build/nexix-multi.iso 128M
test-unit: test-pcc-front-unit
test: test-pcc-front

build/pcc-backend-test.elf: build/crt0.o $(PCC_FRONT_OBJECTS) build/pcc-cgram.o build/pcc-code.o build/pcc-local2.o build/pcc-output.o $(MIP_SHARED) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/pcc-code.d build/pcc-local2.d build/pcc-output.d

.PHONY: test-pcc-integer-unit test-pcc-integer
test-pcc-integer-unit: build/gen-pcc/cgram.c
	$(PYTHON) tests/pcc_integer.py
test-pcc-integer: test-pcc-integer-unit build/pcc-backend-test.elf multi-iso rootfs
	$(PYTHON) tools/pcc_integer_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/pcc_integer_smoke.py $(QEMU) build/nexix-multi.iso 128M
test-unit: test-pcc-integer-unit
test: test-pcc-integer
