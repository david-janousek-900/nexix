# Retained V7 C compiler passes (c0, c1) retargeted to native AMD64.
C_PORT_CFLAGS = $(filter-out -std=c11 -Werror,$(USER_CFLAGS)) -std=gnu99 -Wno-parentheses -Wno-char-subscripts -Wno-unused-parameter -Wno-implicit-fallthrough -Wno-sign-compare -Wno-missing-field-initializers -DNX_SBRK_ARENA
C0_NAMES = c00 c01 c02 c03 c04 c05
C1_NAMES = c10 c11 c12 c13
C2_NAMES = c20 c21
C0_OBJECTS = $(addprefix build/c-,$(addsuffix .o,$(C0_NAMES)))
C2_OBJECTS = $(addprefix build/c-,$(addsuffix .o,$(C2_NAMES)))
C1_OBJECTS = $(addprefix build/c-,$(addsuffix .o,$(C1_NAMES)))
build/c-c0%.o: user/c/c0%.c user/c/c0.h user/c/cpass.h | build/.dir
	$(CC) $(C_PORT_CFLAGS) -c $< -o $@
build/c-c1%.o: user/c/c1%.c user/c/c1.h user/c/cpass.h | build/.dir
	$(CC) $(C_PORT_CFLAGS) -c $< -o $@
build/c-c2%.o: user/c/c2%.c user/c/c2.h | build/.dir
	$(CC) $(C_PORT_CFLAGS) -c $< -o $@
# cvopt converts the code table description into assembler data, as the
# supplied makefile did; the bootstrap build runs a host cvopt.
build/host-cvopt: user/c/cvopt.c | build/.dir
	cc -std=c11 -O2 -Wall -Wextra -Werror $< -o $@
build/gen-c/table.s: user/c/table.s build/host-cvopt
	mkdir -p build/gen-c
	build/host-cvopt user/c/table.s $@
build/c-table.o: build/gen-c/table.s | build/.dir
	$(CC) -c -x assembler $< -o $@
build/c-cvopt.o: user/c/cvopt.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/v7-c0.elf: build/crt0.o $(C0_OBJECTS) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
build/v7-c1.elf: build/crt0.o $(C1_OBJECTS) build/c-table.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
# fc1 is the same native pass with the floating-point startup (V7 linked a
# floating-point simulator); Nexix always uses hardware SSE2.
build/v7-fc1.elf: build/fcrt0.o $(C1_OBJECTS) build/c-table.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
build/v7-c2.elf: build/crt0.o $(C2_OBJECTS) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
build/v7-cvopt.elf: build/crt0.o build/c-cvopt.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include $(C0_OBJECTS:.o=.d) $(C1_OBJECTS:.o=.d) $(C2_OBJECTS:.o=.d) build/c-cvopt.d
.PHONY: test-c-compiler-unit test-c-compiler
test-c-compiler-unit:
	$(PYTHON) tests/c_compiler.py
test-c-compiler: test-c-compiler-unit build/v7-c0.elf build/v7-c1.elf build/v7-c2.elf build/v7-fc1.elf build/v7-cvopt.elf multi-iso rootfs
	$(PYTHON) tools/c_compiler_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/c_compiler_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-c-compiler
test-unit: test-c-compiler-unit

.PHONY: test-cc
test-cc: build/v7-cc.elf build/v7-c0.elf build/v7-c1.elf build/v7-c2.elf build/v7-fc1.elf multi-iso rootfs
	$(PYTHON) tools/cc_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/cc_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-cc
