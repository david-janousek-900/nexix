# Retained PCC front end, privately inspected until native backend/driver closure.
PCC_FRONT_NAMES = scan pftn trees optim xdefs common local numbers ir declarations real abi
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

PCC_PASS2 = build/pcc-code.o build/pcc-local2.o build/pcc-output.o
build/pcc-backend-test.elf: build/crt0.o $(PCC_FRONT_OBJECTS) build/pcc-cgram.o $(PCC_PASS2) build/pcc-ccom.o $(MIP_SHARED) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
# The installed compiler pass and the Fortran second pass (task 196).
build/v7-ccom.elf: build/crt0.o $(PCC_FRONT_OBJECTS) build/pcc-cgram.o $(PCC_PASS2) build/pcc-ccom.o $(MIP_SHARED) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
build/pcc-fort.elf: build/crt0.o $(PCC_FRONT_OBJECTS) build/pcc-cgram.o $(PCC_PASS2) build/pcc-fort.o $(MIP_SHARED) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/pcc-code.d build/pcc-local2.d build/pcc-output.d build/pcc-ccom.d build/pcc-fort.d

.PHONY: test-pcc-integer-unit test-pcc-integer
test-pcc-integer-unit: build/gen-pcc/cgram.c
	$(PYTHON) tests/pcc_integer.py
test-pcc-integer: test-pcc-integer-unit build/pcc-backend-test.elf multi-iso rootfs
	$(PYTHON) tools/pcc_integer_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/pcc_integer_smoke.py $(QEMU) build/nexix-multi.iso 128M
test-unit: test-pcc-integer-unit
test: test-pcc-integer

.PHONY: test-pcc-abi-unit test-pcc-abi
test-pcc-abi-unit: build/gen-pcc/cgram.c
	$(PYTHON) tests/pcc_abi.py
test-pcc-abi: test-pcc-abi-unit build/pcc-backend-test.elf multi-iso rootfs
	$(PYTHON) tools/pcc_abi_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/pcc_abi_smoke.py $(QEMU) build/nexix-multi.iso 128M
test-unit: test-pcc-abi-unit
test: test-pcc-abi

.PHONY: test-pcc-driver-unit test-pcc-driver
test-pcc-driver-unit: build/gen-pcc/cgram.c
	$(PYTHON) tests/pcc_driver.py
test-pcc-driver: test-pcc-driver-unit build/v7-ccom.elf build/pcc-fort.elf build/v7-c2.elf multi-iso rootfs
	$(PYTHON) tools/pcc_driver_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/pcc_driver_smoke.py $(QEMU) build/nexix-multi.iso 128M
test-unit: test-pcc-driver-unit
test: test-pcc-driver
