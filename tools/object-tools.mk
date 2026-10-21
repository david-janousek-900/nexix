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
build/v7-lorder.o: user/v7/cmd/lorder.c user/toolchain/object.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/toolchain -c $< -o $@
build/v7-lorder-native.elf: build/crt0.o build/v7-lorder.o build/toolchain-object.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/v7-lorder.d
.PHONY: test-lorder-unit test-lorder
test-lorder-unit:
	$(PYTHON) tests/object_tools.py lorder
test-lorder: test-lorder-unit multi-iso rootfs
	$(PYTHON) tools/object_tools_smoke.py $(QEMU) build/nexix-multi.iso 16M lorder
	$(PYTHON) tools/object_tools_smoke.py $(QEMU) build/nexix-multi.iso 128M lorder
test: test-lorder
test-unit: test-lorder-unit

# V7 cpp scanner with a native LP64, short-circuit expression evaluator.
CPP_PORT_FLAGS = $(filter-out -std=c11 -Werror,$(USER_CFLAGS)) -std=gnu89 -funsigned-char -fcommon -Wno-comment -Wno-implicit-int -Wno-return-type
build/cpp-scanner.o: user/cpp/cpp.c | build/.dir
	$(CC) $(CPP_PORT_FLAGS) -c $< -o $@
build/cpp-expr.o: user/cpp/expr.c | build/.dir
	$(CC) $(CPP_PORT_FLAGS) -c $< -o $@
build/v7-cpp.elf: build/crt0.o build/cpp-scanner.o build/cpp-expr.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/cpp-scanner.d build/cpp-expr.d
.PHONY: test-cpp-unit test-cpp
test-cpp-unit:
	$(PYTHON) tests/cpp.py
test-cpp: test-cpp-unit multi-iso rootfs
	$(PYTHON) tools/cpp_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/cpp_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-cpp
test-unit: test-cpp-unit

build/toolchain-archive.o: user/toolchain/archive.c user/toolchain/archive.h user/toolchain/object.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/toolchain -c $< -o $@
build/v7-ar.o: user/v7/cmd/ar.c user/toolchain/archive.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/toolchain -c $< -o $@
build/v7-ar.elf: build/crt0.o build/v7-ar.o build/toolchain-archive.o build/toolchain-object.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/toolchain-archive.d build/v7-ar.d
.PHONY: test-ar-unit test-ar
test-ar-unit:
	$(PYTHON) tests/archives.py ar
test-ar: test-ar-unit multi-iso rootfs
	$(PYTHON) tools/archives_smoke.py $(QEMU) build/nexix-multi.iso 16M ar
	$(PYTHON) tools/archives_smoke.py $(QEMU) build/nexix-multi.iso 128M ar
test: test-ar
test-unit: test-ar-unit

build/v7-arcv.o: user/v7/cmd/arcv.c user/toolchain/archive.h | build/.dir
	$(CC) $(USER_CFLAGS) -Iuser/toolchain -c $< -o $@
build/v7-arcv.elf: build/crt0.o build/v7-arcv.o build/toolchain-archive.o build/toolchain-object.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include build/v7-arcv.d
.PHONY: test-arcv-unit test-arcv
test-arcv-unit:
	$(PYTHON) tests/archives.py arcv
test-arcv: test-arcv-unit multi-iso rootfs
	$(PYTHON) tools/archives_smoke.py $(QEMU) build/nexix-multi.iso 16M arcv
	$(PYTHON) tools/archives_smoke.py $(QEMU) build/nexix-multi.iso 128M arcv
test: test-arcv
test-unit: test-arcv-unit
