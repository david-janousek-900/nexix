TOOLCHAIN_PREFIX ?= x86_64-elf-
CC = $(TOOLCHAIN_PREFIX)gcc
LD = $(TOOLCHAIN_PREFIX)ld
AR = $(TOOLCHAIN_PREFIX)ar
OBJCOPY = $(TOOLCHAIN_PREFIX)objcopy
PYTHON ?= python3
QEMU ?= qemu-system-x86_64
CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -fno-pie -m64 -mno-red-zone -mgeneral-regs-only -fno-asynchronous-unwind-tables -Ikernel/include -Iinclude -MMD -MP
OBJECTS = build/boot.o build/main.o build/serial.o build/interrupts.o build/interrupt_stubs.o build/boot_info.o build/memory.o build/paging.o build/paging_test.o build/segments.o build/user_entry.o build/user_blob.o build/user_test.o build/process.o build/context.o build/process_test.o build/process_blob.o build/uaccess.o build/syscall.o build/syscall_test.o build/syscall_blob.o build/elf.o build/exec.o build/exec_test.o build/exec_image.o build/libc_test.o build/libc_image.o build/block.o build/v7fs.o build/fs_test.o build/root_image.o build/files.o build/files_test.o build/files_image.o build/tty.o build/tty_test.o build/tty_image.o build/signals.o build/signals_test.o build/signals_image.o build/catalog.o build/v7_userland_test.o build/v7_test_image.o
HOST_CC ?= cc
.DEFAULT_GOAL := all
-include $(OBJECTS:.o=.d)

.PHONY: all check iso run smoke test test-unit clean
all: build/nexix.elf

build:
	mkdir -p $@

build/boot.o: kernel/arch/x86_64/boot.S | build
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@

build/main.o: kernel/main.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/serial.o: kernel/serial.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/boot_info.o: kernel/boot.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/memory.o: kernel/memory.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/paging.o: kernel/arch/x86_64/paging.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/paging_test.o: kernel/paging_test.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/segments.o: kernel/arch/x86_64/segments.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/uaccess.o: kernel/uaccess.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/syscall.o: kernel/v7/syscall.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/syscall_test.o: kernel/syscall_test.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/syscall_blob.o: kernel/arch/x86_64/syscall_test.S include/nexix/syscall.h include/nexix/errno.h | build
	$(CC) -m64 -ffreestanding -fno-pie -Iinclude -c $< -o $@

build/process.o: kernel/v7/process.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/process_test.o: kernel/process_test.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/context.o: kernel/arch/x86_64/context.S | build
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@

build/process_blob.o: kernel/arch/x86_64/process_test.S | build
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@

build/user_test.o: kernel/user_test.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/user_entry.o: kernel/arch/x86_64/user_entry.S | build
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@

build/user_blob.o: kernel/arch/x86_64/user_test.S | build
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@

build/interrupts.o: kernel/arch/x86_64/interrupts.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/interrupt_stubs.o: kernel/arch/x86_64/interrupts.S | build
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@

build/nexix.elf: $(OBJECTS) kernel/arch/x86_64/linker.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T kernel/arch/x86_64/linker.ld $(OBJECTS) -o $@

check: build/nexix.elf
	$(PYTHON) tools/check_kernel.py $<

build/nexix.iso: build/nexix.elf boot/grub.cfg
	mkdir -p build/iso/boot/grub
	cp build/nexix.elf build/iso/boot/nexix.elf
	cp boot/grub.cfg build/iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ build/iso

iso: check build/nexix.iso
run: iso disk
	$(QEMU) -machine pc -accel tcg -cpu qemu64 -smp 1 -m 128M -rtc base=utc -boot d -drive file=build/nexix.img,format=raw,if=ide,index=0 -cdrom build/nexix.iso -display none -chardev stdio,id=console,mux=on,signal=off -serial chardev:console -mon chardev=console,mode=readline -no-reboot

smoke: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso

build/test-%.iso: build/nexix.elf boot/grub.cfg
	mkdir -p build/iso-$*/boot/grub
	cp build/nexix.elf build/iso-$*/boot/nexix.elf
	sed 's|/boot/nexix.elf|/boot/nexix.elf test=$*|' boot/grub.cfg > build/iso-$*/boot/grub/grub.cfg
	grub-mkrescue -o $@ build/iso-$*

build/test-memory.iso: build/nexix.elf boot/grub.cfg
	mkdir -p build/iso-memory/boot/grub
	cp build/nexix.elf build/iso-memory/boot/nexix.elf
	$(PYTHON) -c 'from pathlib import Path; Path("build/iso-memory/boot/reservation.bin").write_bytes(bytes([0xa5]) * 8193)'
	sed 's|/boot/nexix.elf|/boot/nexix.elf test=memory|' boot/grub.cfg | sed '/multiboot2/a\    module2 /boot/reservation.bin allocator-test' > build/iso-memory/boot/grub/grub.cfg
	grub-mkrescue -o $@ build/iso-memory

build/memory-test: tests/memory.c kernel/boot.c kernel/memory.c kernel/include/boot.h kernel/include/memory.h | build
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include tests/memory.c kernel/boot.c kernel/memory.c -o $@

build/v7-types-test: tests/v7_types.c kernel/include/v7/constants.h kernel/include/v7/types.h kernel/include/v7/disk.h kernel/include/v7/core.h kernel/include/paging.h kernel/include/memory.h | build
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include $< -o $@

build/uaccess-test: tests/uaccess.c kernel/uaccess.c kernel/include/uaccess.h kernel/include/process.h kernel/include/paging.h kernel/include/memory.h kernel/include/boot.h kernel/include/v7/core.h kernel/include/v7/types.h kernel/include/v7/disk.h kernel/include/v7/constants.h include/nexix/errno.h | build
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/uaccess.c kernel/uaccess.c -o $@

test-unit: build/memory-test build/v7-types-test build/uaccess-test build/filesystem-test build/tty-unit-test build/root-test.img
	./build/ide-test
	./build/memory-test
	./build/v7-types-test
	./build/uaccess-test
	./build/filesystem-test build/root-test.img
	./build/tty-unit-test

test: test-unit smoke build/test-ud.iso build/test-pf.iso build/test-memory.iso test-paging test-user test-fs test-files test-tty test-signals test-v7
	$(PYTHON) tools/smoke.py $(QEMU) build/test-ud.iso ud
	$(PYTHON) tools/smoke.py $(QEMU) build/test-pf.iso pf
	$(PYTHON) tools/smoke.py $(QEMU) build/test-memory.iso memory 16M
	$(PYTHON) tools/smoke.py $(QEMU) build/test-memory.iso memory 128M

.PHONY: test-paging
test-paging: check build/test-wp.iso build/test-nx.iso build/test-unmap.iso build/test-null.iso build/test-paging-oom.iso build/test-protect.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-wp.iso wp
	$(PYTHON) tools/smoke.py $(QEMU) build/test-nx.iso nx
	$(PYTHON) tools/smoke.py $(QEMU) build/test-unmap.iso unmap
	$(PYTHON) tools/smoke.py $(QEMU) build/test-null.iso null
	$(PYTHON) tools/smoke.py $(QEMU) build/test-paging-oom.iso paging-oom 16M
	$(PYTHON) tools/smoke.py $(QEMU) build/test-protect.iso protect

.PHONY: test-user
test-user: check build/test-user-kernel.iso build/test-user-ro.iso build/test-user-nx.iso build/test-user-priv.iso build/test-user-io.iso build/test-user-int.iso build/test-user-bad-stack.iso build/test-user-df.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-user-kernel.iso user-kernel
	$(PYTHON) tools/smoke.py $(QEMU) build/test-user-ro.iso user-ro
	$(PYTHON) tools/smoke.py $(QEMU) build/test-user-nx.iso user-nx
	$(PYTHON) tools/smoke.py $(QEMU) build/test-user-priv.iso user-priv
	$(PYTHON) tools/smoke.py $(QEMU) build/test-user-io.iso user-io
	$(PYTHON) tools/smoke.py $(QEMU) build/test-user-int.iso user-int
	$(PYTHON) tools/smoke.py $(QEMU) build/test-user-bad-stack.iso user-bad-stack
	$(PYTHON) tools/smoke.py $(QEMU) build/test-user-df.iso user-df

clean:
	$(PYTHON) -c 'from pathlib import Path; import shutil; p=Path("build"); [(shutil.rmtree(q) if q.is_dir() else q.unlink()) for q in p.iterdir() if q.name != "nexix.img"] if p.exists() else None'

.PHONY: test-process
test-process: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso process 16M

.PHONY: test-syscall
test-syscall: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso syscall 16M

# A real, statically linked ELF64 executable; no host libc or runtime.
build/exec_program.o: user/exec-test.S include/nexix/syscall.h include/nexix/errno.h | build
	$(CC) -m64 -ffreestanding -fno-pie -Iinclude -c $< -o $@
build/exec-test.elf: build/exec_program.o user/exec-test.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/exec-test.ld $< -o $@
build/exec_image.o: build/exec-test.elf
	$(LD) -r -b binary $< -o build/exec_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/exec_raw.o $@
build/elf.o: kernel/elf.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/exec.o: kernel/v7/exec.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/exec_test.o: kernel/exec_test.c | build
	$(CC) $(CFLAGS) -c $< -o $@
.PHONY: test-exec
test-exec: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso exec 16M

# Native userland: compiler freestanding headers plus Nexix headers only.
USER_CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -fno-pie -m64 -mno-red-zone -mgeneral-regs-only -fno-asynchronous-unwind-tables -nostdinc -isystem $(shell $(CC) -print-file-name=include) -Ilibc/include -Iinclude -MMD -MP
LIBC_OBJECTS = build/libc_syscall.o build/libc_syscalls.o build/libc_string.o build/libc_stdlib.o build/libc_signal.o build/libc_setjmp.o build/libc_stdio.o build/libc_malloc.o build/libc_exec.o build/libc_time.o build/libc_sleep.o
-include $(LIBC_OBJECTS:.o=.d) build/libc_program.d
build/crt0.o: libc/arch/x86_64/crt0.S | build
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
build/libc_syscall.o: libc/arch/x86_64/syscall.S | build
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
build/libc_%.o: libc/%.c | build
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/libc.a: $(LIBC_OBJECTS)
	$(AR) rcs $@ $^
build/libc_program.o: user/libc-test.c | build
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/libc_abi.o: user/libc-test-abi.S include/nexix/syscall.h | build
	$(CC) -m64 -ffreestanding -fno-pie -Iinclude -c $< -o $@
build/libc-test.elf: build/crt0.o build/libc_program.o build/libc_abi.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/libc_program.o build/libc_abi.o build/libc.a -o $@
build/libc_image.o: build/libc-test.elf
	$(LD) -r -b binary $< -o build/libc_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/libc_raw.o $@
build/libc_test.o: kernel/libc_test.c | build
	$(CC) $(CFLAGS) -c $< -o $@
.PHONY: userland test-libc
userland: build/crt0.o build/libc.a build/libc-test.elf
test-libc: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso libc 16M

# Small V7 filesystem fixture; a bootable init/shell root is step 17.
build/root-test.img: tools/mk_v7fs.py build/exec-test.elf build/libc-test.elf
	$(PYTHON) tools/mk_v7fs.py --exec build/exec-test.elf --libc build/libc-test.elf --output $@
build/root_image.o: build/root-test.img
	$(LD) -r -b binary $< -o build/root_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/root_raw.o $@
build/block.o: kernel/block.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/v7fs.o: kernel/v7/fs.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/fs_test.o: kernel/fs_test.c | build
	$(CC) $(CFLAGS) -c $< -o $@
.PHONY: test-fs
test-fs: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso fs 16M

build/filesystem-test: tests/filesystem.c kernel/block.c kernel/v7/fs.c kernel/include/block.h kernel/include/v7/fs.h kernel/include/v7/disk.h kernel/include/v7/types.h kernel/include/v7/constants.h include/nexix/errno.h | build
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/filesystem.c kernel/block.c kernel/v7/fs.c -o $@

# Step 13: production file syscalls exercised by a native C executable.
build/files.o: kernel/v7/files.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/files_test.o: kernel/files_test.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/files_program.o: user/files-test.c | build
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/files_program.d
build/files-test.elf: build/crt0.o build/files_program.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/files_program.o build/libc.a -o $@
build/files_image.o: build/files-test.elf
	$(LD) -r -b binary $< -o build/files_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/files_raw.o $@
.PHONY: test-files
test-files: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso files 16M
userland: build/files-test.elf

# Step 14: serial terminal and native sgtty/ioctl userland fixture.
build/tty.o: kernel/v7/tty.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/tty_test.o: kernel/tty_test.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/tty_program.o: user/tty-test.c | build
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/tty_program.d
build/tty-test.elf: build/crt0.o build/tty_program.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/tty_program.o build/libc.a -o $@
build/tty_image.o: build/tty-test.elf
	$(LD) -r -b binary $< -o build/tty_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/tty_raw.o $@
userland: build/tty-test.elf

.PHONY: test-tty run-tty
test-tty: check build/test-tty.iso build/test-tty-demo.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-tty.iso tty 16M
	$(PYTHON) tools/smoke.py $(QEMU) build/test-tty.iso tty 128M
	$(PYTHON) tools/smoke.py $(QEMU) build/test-tty-demo.iso tty-demo 16M
run-tty: check build/test-tty-demo.iso
	$(QEMU) -machine pc -accel tcg -cpu qemu64 -smp 1 -m 128M -cdrom build/test-tty-demo.iso -display none -chardev stdio,id=console,mux=on,signal=off -serial chardev:console -mon chardev=console,mode=readline -no-reboot

build/tty-unit-test: tests/tty.c kernel/v7/tty.c kernel/include/tty.h kernel/include/serial.h kernel/include/uaccess.h kernel/include/process.h kernel/include/v7/core.h include/nexix/tty.h include/nexix/errno.h | build
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/tty.c kernel/v7/tty.c -o $@

# Step 15: signals and the remaining init/shell syscall fixture.
build/signals.o: kernel/v7/signals.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/signals_test.o: kernel/signals_test.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/libc_signal.o: libc/arch/x86_64/signal.S include/nexix/syscall.h | build
	$(CC) -m64 -ffreestanding -fno-pie -Iinclude -c $< -o $@
build/signals_program.o: user/signals-test.c | build
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/signals_program.d
build/signals-test.elf: build/crt0.o build/signals_program.o build/signals_abi.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/signals_program.o build/signals_abi.o build/libc.a -o $@
build/signals_image.o: build/signals-test.elf
	$(LD) -r -b binary $< -o build/signals_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/signals_raw.o $@
userland: build/signals-test.elf
.PHONY: test-signals
test-signals: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso signals 16M
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso signals 128M

build/libc_setjmp.o: libc/arch/x86_64/setjmp.S | build
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
build/signals_abi.o: user/signals-test-abi.S include/nexix/syscall.h | build
	$(CC) -m64 -ffreestanding -fno-pie -Iinclude -c $< -o $@

# Native builds of retained V7 K&R sources; original reference/ stays pristine.
V7_CFLAGS = $(filter-out -std=c11 -Werror,$(USER_CFLAGS)) -std=gnu89 -funsigned-char -fcommon -fno-strict-aliasing -Wno-implicit-int -Wno-implicit-function-declaration -Wno-return-type -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-pointer-sign -Wno-unused-parameter -Wno-parentheses -Wno-missing-field-initializers
V7_COMMANDS = init echo cat ls pwd cp ln rm
V7_SH_SOURCES = $(wildcard user/v7/sh/*.c)
V7_SH_OBJECTS = $(patsubst user/v7/sh/%.c,build/v7-sh-%.o,$(V7_SH_SOURCES))
V7_ELFS = build/v7-sh.elf $(addprefix build/v7-,$(addsuffix .elf,$(V7_COMMANDS)))
-include $(V7_SH_OBJECTS:.o=.d) $(addprefix build/v7-,$(addsuffix .d,$(V7_COMMANDS)))
build/v7-sh-%.o: user/v7/sh/%.c | build
	$(CC) $(V7_CFLAGS) -c $< -o $@
build/v7-%.o: user/v7/cmd/%.c user/v7/compat.h | build
	$(CC) $(V7_CFLAGS) -include user/v7/compat.h -c $< -o $@
build/v7-sh.elf: build/crt0.o $(V7_SH_OBJECTS) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o $(V7_SH_OBJECTS) build/libc.a -o $@
build/v7-%.elf: build/crt0.o build/v7-%.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/v7-$*.o build/libc.a -o $@
.PHONY: v7-userland
v7-userland: $(V7_ELFS)
userland: v7-userland

V7_IMAGES = $(addprefix build/v7_image_,$(addsuffix .o,sh init echo cat ls pwd cp ln rm))
# Embedded executable images for the step 16 acceptance fixture.
OBJECTS += $(V7_IMAGES)
-include build/catalog.d build/v7_userland_test.d build/v7_test_program.d
build/catalog.o: kernel/v7/catalog.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/v7_userland_test.o: kernel/v7_userland_test.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/v7_image_%.o: build/v7-%.elf
	$(LD) -r -b binary $< -o build/v7_raw_$*.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/v7_raw_$*.o $@
build/v7_test_program.o: user/v7-test.c | build
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/v7-test.elf: build/crt0.o build/v7_test_program.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/v7_test_program.o build/libc.a -o $@
build/v7_test_image.o: build/v7-test.elf
	$(LD) -r -b binary $< -o build/v7_test_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/v7_test_raw.o $@
.PHONY: test-v7
test-v7: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso v7 16M
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso v7 128M
build/nexix.elf: $(V7_IMAGES)

.SECONDARY: $(addprefix build/v7-,$(addsuffix .o,$(V7_COMMANDS)))

# Packaged 0.1.0 root, separate from the filesystem regression fixture.
.PHONY: rootfs test-rootfs
rootfs: build/root.img
build/root.img: tools/mk_rootfs.py tools/mk_v7fs.py $(V7_ELFS) rootfs/etc/rc rootfs/etc/ttys rootfs/etc/passwd rootfs/profile
	$(PYTHON) tools/mk_rootfs.py --output $@
build/boot_root_image.o: build/root.img
	$(LD) -r -b binary $< -o build/boot_root_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/boot_root_raw.o $@
build/rootfs_test.o: kernel/rootfs_test.c | build
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/boot_root_image.o build/rootfs_test.o
build/nexix.elf: build/boot_root_image.o build/rootfs_test.o
-include build/rootfs_test.d
test-rootfs: check build/test-selftest.iso test-root-image
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso rootfs 16M
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso rootfs 128M
test: test-rootfs

.PHONY: test-root-image
test-root-image: build/root.img
	$(PYTHON) tests/root_image.py
test-unit: test-root-image

build/root_boot.o: kernel/root_boot.c | build
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/root_boot.o
build/nexix.elf: build/root_boot.o
-include build/root_boot.d

build/root_exec_program.o: user/root-exec-test.c | build
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/root-exec-test.elf: build/crt0.o build/root_exec_program.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/root_exec_program.o build/libc.a -o $@
build/root_exec_image.o: build/root-exec-test.elf
	$(LD) -r -b binary $< -o build/root_exec_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/root_exec_raw.o $@
OBJECTS += build/root_exec_image.o
build/nexix.elf: build/root_exec_image.o
-include build/root_exec_program.d
.PHONY: test-boot
test-boot: iso
	$(PYTHON) tools/boot_smoke.py $(QEMU) build/nexix.iso 16M
	$(PYTHON) tools/boot_smoke.py $(QEMU) build/nexix.iso 128M
test: test-boot

.PHONY: test-shell
test-shell: iso
	$(PYTHON) tools/interactive_smoke.py $(QEMU) build/nexix.iso 16M
	$(PYTHON) tools/interactive_smoke.py $(QEMU) build/nexix.iso 128M
test: test-shell

# Preserve the working disk across builds; root.img is the seed.
.PHONY: disk test-storage
disk: build/root.img
	$(PYTHON) -c 'from pathlib import Path; p=Path("build/nexix.img"); p.exists() or p.write_bytes(Path("build/root.img").read_bytes())'
build/ide.o: kernel/ide.c | build
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/ide.o
build/nexix.elf: build/ide.o
-include build/ide.d
test-storage: iso
	$(PYTHON) tools/storage_smoke.py $(QEMU) build/nexix.iso 16M
	$(PYTHON) tools/storage_smoke.py $(QEMU) build/nexix.iso 128M
test: test-storage
build/ide-test: tests/ide.c tests/ide/io.h kernel/ide.c kernel/block.c kernel/include/ide.h kernel/include/block.h include/nexix/errno.h | build
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Itests/ide -Ikernel/include -Iinclude tests/ide.c kernel/ide.c kernel/block.c -o $@
test-unit: build/ide-test

# Step 21: read-only source inventory and coverage checks.
.PHONY: test-inventory
test-inventory:
	$(PYTHON) tools/check_inventory.py
	$(PYTHON) tests/inventory.py
test: test-inventory

# Step 22: boot RTC and shared epoch clock.
build/rtc.o: kernel/arch/x86_64/rtc.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/clock.o: kernel/clock.c | build
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/rtc.o build/clock.o
build/nexix.elf: build/rtc.o build/clock.o
-include build/rtc.d build/clock.d
build/rtc-test: tests/rtc.c tests/rtc/io.h kernel/arch/x86_64/rtc.c kernel/clock.c kernel/include/clock.h | build
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Itests/rtc -Ikernel/include -Iinclude tests/rtc.c kernel/arch/x86_64/rtc.c kernel/clock.c -o $@
.PHONY: test-rtc
test-rtc: build/rtc-test iso
	./build/rtc-test
	$(PYTHON) tools/rtc_smoke.py $(QEMU) build/nexix.iso
test: test-rtc
