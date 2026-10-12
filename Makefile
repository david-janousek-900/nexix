# Host GCC/binutils are the default bootstrap tools on x86_64 Linux.
# Override with TOOLCHAIN_PREFIX=x86_64-elf- for a cross-toolchain.
TOOLCHAIN_PREFIX ?=
CC = $(TOOLCHAIN_PREFIX)gcc
LD = $(TOOLCHAIN_PREFIX)ld
AR = $(TOOLCHAIN_PREFIX)ar
OBJCOPY = $(TOOLCHAIN_PREFIX)objcopy
PYTHON ?= python3
QEMU ?= qemu-system-x86_64
DISK_IMAGE ?= build/nexix.img
CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -fno-pie -m64 -mno-red-zone -mgeneral-regs-only -fno-asynchronous-unwind-tables -Ikernel/include -Iinclude -MMD -MP
OBJECTS = build/boot.o build/main.o build/serial.o build/interrupts.o build/interrupt_stubs.o build/boot_info.o build/memory.o build/paging.o build/paging_test.o build/segments.o build/user_entry.o build/user_blob.o build/user_test.o build/process.o build/context.o build/process_test.o build/process_blob.o build/uaccess.o build/syscall.o build/syscall_test.o build/syscall_blob.o build/elf.o build/exec.o build/exec_test.o build/exec_image.o build/libc_test.o build/libc_image.o build/block.o build/v7fs.o build/fs_test.o build/root_image.o build/files.o build/files_test.o build/files_image.o build/tty.o build/tty_test.o build/tty_image.o build/signals.o build/signals_test.o build/signals_image.o build/catalog.o build/v7_userland_test.o build/v7_test_image.o
HOST_CC ?= cc
.DEFAULT_GOAL := all
-include $(OBJECTS:.o=.d)

.PHONY: all build check iso run smoke test test-unit clean
all: build/nexix.elf

build/.dir:
	mkdir -p build
	touch $@

build/boot.o: kernel/arch/x86_64/boot.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@

build/main.o: kernel/main.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/serial.o: kernel/serial.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/boot_info.o: kernel/boot.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/memory.o: kernel/memory.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/paging.o: kernel/arch/x86_64/paging.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/paging_test.o: kernel/paging_test.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/segments.o: kernel/arch/x86_64/segments.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/uaccess.o: kernel/uaccess.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/syscall.o: kernel/v7/syscall.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/syscall_test.o: kernel/syscall_test.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/syscall_blob.o: kernel/arch/x86_64/syscall_test.S include/nexix/syscall.h include/nexix/errno.h | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -Iinclude -c $< -o $@

build/process.o: kernel/v7/process.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/process_test.o: kernel/process_test.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/context.o: kernel/arch/x86_64/context.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@

build/process_blob.o: kernel/arch/x86_64/process_test.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@

build/user_test.o: kernel/user_test.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/user_entry.o: kernel/arch/x86_64/user_entry.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@

build/user_blob.o: kernel/arch/x86_64/user_test.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@

build/interrupts.o: kernel/arch/x86_64/interrupts.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/interrupt_stubs.o: kernel/arch/x86_64/interrupts.S | build/.dir
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
build: iso disk
run: build
	$(QEMU) -machine pc -accel tcg -cpu qemu64 -smp 1 -m 128M -rtc base=utc -boot d -drive "file=$(DISK_IMAGE),format=raw,if=ide,index=0" -cdrom build/nexix.iso -display none -chardev stdio,id=console,mux=on,signal=off -serial chardev:console -mon chardev=console,mode=readline -no-reboot

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

build/memory-test: tests/memory.c kernel/boot.c kernel/memory.c kernel/include/boot.h kernel/include/memory.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include tests/memory.c kernel/boot.c kernel/memory.c -o $@

build/v7-types-test: tests/v7_types.c kernel/include/v7/constants.h kernel/include/v7/types.h kernel/include/v7/disk.h kernel/include/v7/core.h kernel/include/paging.h kernel/include/memory.h include/nexix/fpu.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $< -o $@

build/uaccess-test: tests/uaccess.c kernel/uaccess.c kernel/include/uaccess.h kernel/include/process.h kernel/include/paging.h kernel/include/memory.h kernel/include/boot.h kernel/include/v7/core.h kernel/include/v7/types.h kernel/include/v7/disk.h kernel/include/v7/constants.h include/nexix/errno.h | build/.dir
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
	rm -rf -- build

.PHONY: test-process
test-process: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso process 16M

.PHONY: test-syscall
test-syscall: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso syscall 16M

# A real, statically linked ELF64 executable; no host libc or runtime.
build/exec_program.o: user/exec-test.S include/nexix/syscall.h include/nexix/errno.h | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -Iinclude -c $< -o $@
build/exec-test.elf: build/exec_program.o user/exec-test.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/exec-test.ld $< -o $@
build/exec_image.o: build/exec-test.elf
	$(LD) -r -b binary $< -o build/exec_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/exec_raw.o $@
build/elf.o: kernel/elf.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/exec.o: kernel/v7/exec.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/exec_test.o: kernel/exec_test.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
.PHONY: test-exec
test-exec: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso exec 16M

# Native userland: compiler freestanding headers plus Nexix headers only.
USER_CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -fno-pie -m64 -mno-red-zone -mno-avx -fno-asynchronous-unwind-tables -nostdinc -isystem $(shell $(CC) -print-file-name=include) -Ilibc/include -Iinclude -MMD -MP
LIBC_OBJECTS = build/libc_syscall.o build/libc_syscalls.o build/libc_string.o build/libc_stdlib.o build/libc_signal.o build/libc_setjmp.o build/libc_stdio.o build/libc_malloc.o build/libc_exec.o build/libc_time.o build/libc_sleep.o build/libc_float.o build/libc_convert.o build/libc_scan.o build/libc_ctype.o build/libc_numbers.o build/libc_bytes.o build/libc_gcvt.o build/libc_abort.o build/libc_mktemp.o build/libc_errors.o build/libc_perror.o build/libc_passwd.o build/libc_group.o build/libc_crypt.o build/libc_terminal.o build/libc_getpass.o build/libc_system.o build/libc_popen.o
-include $(LIBC_OBJECTS:.o=.d) build/libc_program.d
build/crt0.o: libc/arch/x86_64/crt0.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
build/libc_syscall.o: libc/arch/x86_64/syscall.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
build/libc_%.o: libc/%.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/libc.a: $(LIBC_OBJECTS)
	$(AR) rcs $@ $^
build/libc_program.o: user/libc-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/libc_abi.o: user/libc-test-abi.S include/nexix/syscall.h | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -Iinclude -c $< -o $@
build/libc-test.elf: build/crt0.o build/libc_program.o build/libc_scan_program.o build/libc_helpers_program.o build/crypt_program.o build/libc_abi.o build/fpu_program.o build/libc.a build/libm.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/libc_program.o build/libc_scan_program.o build/libc_helpers_program.o build/crypt_program.o build/libc_abi.o build/fpu_program.o build/libc.a build/libm.a build/libc.a -o $@
build/libc_image.o: build/libc-test.elf
	$(LD) -r -b binary $< -o build/libc_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/libc_raw.o $@
build/libc_test.o: kernel/libc_test.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
.PHONY: userland test-libc
userland: build/crt0.o build/libc.a build/libc-test.elf
test-libc: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso libc 16M

# Small V7 filesystem fixture; a bootable init/shell root is task 17.
build/root-test.img: tools/mk_v7fs.py build/exec-test.elf build/libc-test.elf
	$(PYTHON) tools/mk_v7fs.py --exec build/exec-test.elf --libc build/libc-test.elf --output $@
build/root_image.o: build/root-test.img
	$(LD) -r -b binary $< -o build/root_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/root_raw.o $@
build/block.o: kernel/block.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/v7fs.o: kernel/v7/fs.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/fs_test.o: kernel/fs_test.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
.PHONY: test-fs
test-fs: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso fs 16M

build/filesystem-test: tests/filesystem.c kernel/block.c kernel/v7/fs.c kernel/include/clock.h kernel/include/block.h kernel/include/v7/fs.h kernel/include/v7/disk.h kernel/include/v7/types.h kernel/include/v7/constants.h include/nexix/errno.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/filesystem.c kernel/block.c kernel/v7/fs.c -o $@

# Task 13: production file syscalls exercised by a native C executable.
build/files.o: kernel/v7/files.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/files_test.o: kernel/files_test.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/files_program.o: user/files-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/files_program.d
build/files-test.elf: build/crt0.o build/files_program.o build/stdio_program.o build/stdio_compat_program.o build/mktemp_program.o build/perror_program.o build/passwd_program.o build/group_program.o build/terminal_account_program.o build/v7_stdio_fragments.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/files_program.o build/stdio_program.o build/stdio_compat_program.o build/mktemp_program.o build/perror_program.o build/passwd_program.o build/group_program.o build/terminal_account_program.o build/v7_stdio_fragments.o build/libc.a -o $@
build/files_image.o: build/files-test.elf
	$(LD) -r -b binary $< -o build/files_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/files_raw.o $@
.PHONY: test-files
test-files: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso files 16M
userland: build/files-test.elf

# Task 14: serial terminal and native sgtty/ioctl userland fixture.
build/tty.o: kernel/v7/tty.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/tty_test.o: kernel/tty_test.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/tty_program.o: user/tty-test.c | build/.dir
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

build/tty-unit-test: tests/tty.c kernel/v7/tty.c kernel/include/tty.h kernel/include/serial.h kernel/include/uaccess.h kernel/include/process.h kernel/include/v7/core.h include/nexix/tty.h include/nexix/errno.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/tty.c kernel/v7/tty.c -o $@

# Task 15: signals and the remaining init/shell syscall fixture.
build/signals.o: kernel/v7/signals.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/signals_test.o: kernel/signals_test.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/libc_signal.o: libc/arch/x86_64/signal.S include/nexix/syscall.h | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -Iinclude -c $< -o $@
build/signals_program.o: user/signals-test.c | build/.dir
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

build/libc_setjmp.o: libc/arch/x86_64/setjmp.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
build/signals_abi.o: user/signals-test-abi.S include/nexix/syscall.h | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -Iinclude -c $< -o $@

# Native builds of retained V7 K&R sources; original reference/ stays pristine.
V7_CFLAGS = $(filter-out -std=c11 -Werror,$(USER_CFLAGS)) -std=gnu89 -funsigned-char -fcommon -fno-strict-aliasing -Wno-implicit-int -Wno-implicit-function-declaration -Wno-return-type -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-pointer-sign -Wno-unused-parameter -Wno-parentheses -Wno-missing-field-initializers
V7_COMMANDS = init echo cat ls pwd cp ln rm date touch
V7_SH_SOURCES = $(wildcard user/v7/sh/*.c)
V7_SH_OBJECTS = $(patsubst user/v7/sh/%.c,build/v7-sh-%.o,$(V7_SH_SOURCES))
V7_ELFS = build/v7-sh.elf $(addprefix build/v7-,$(addsuffix .elf,$(V7_COMMANDS)))
-include $(V7_SH_OBJECTS:.o=.d) $(addprefix build/v7-,$(addsuffix .d,$(V7_COMMANDS)))
build/v7-sh-%.o: user/v7/sh/%.c | build/.dir
	$(CC) $(V7_CFLAGS) -c $< -o $@
build/v7-%.o: user/v7/cmd/%.c user/v7/compat.h | build/.dir
	$(CC) $(V7_CFLAGS) -include user/v7/compat.h -c $< -o $@
build/v7-sh.elf: build/crt0.o $(V7_SH_OBJECTS) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o $(V7_SH_OBJECTS) build/libc.a -o $@
build/v7-%.elf: build/crt0.o build/v7-%.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/v7-$*.o build/libc.a -o $@
.PHONY: v7-userland
v7-userland: $(V7_ELFS)
userland: v7-userland

V7_IMAGES = $(addprefix build/v7_image_,$(addsuffix .o,sh init echo cat ls pwd cp ln rm date touch))
# Embedded executable images for the task 16 acceptance fixture.
OBJECTS += $(V7_IMAGES)
-include build/catalog.d build/v7_userland_test.d build/v7_test_program.d
build/catalog.o: kernel/v7/catalog.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/v7_userland_test.o: kernel/v7_userland_test.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/v7_image_%.o: build/v7-%.elf
	$(LD) -r -b binary $< -o build/v7_raw_$*.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/v7_raw_$*.o $@
build/v7_test_program.o: user/v7-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/v7-test.elf: build/crt0.o build/v7_test_program.o build/system_program.o build/popen_program.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/v7_test_program.o build/system_program.o build/popen_program.o build/libc.a -o $@
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
build/rootfs_test.o: kernel/rootfs_test.c | build/.dir
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

build/root_boot.o: kernel/root_boot.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/root_boot.o
build/nexix.elf: build/root_boot.o
-include build/root_boot.d

build/root_exec_program.o: user/root-exec-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/root-exec-test.elf: build/crt0.o build/root_exec_program.o build/setid_program.o build/text_program.o build/core_program.o build/core_fault.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/root_exec_program.o build/setid_program.o build/text_program.o build/core_program.o build/core_fault.o build/libc.a -o $@
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
	$(PYTHON) tools/interactive_smoke.py $(QEMU) build/nexix.iso 16M ide
	$(PYTHON) tools/interactive_smoke.py $(QEMU) build/nexix.iso 128M ide
test: test-shell

# Preserve the working disk across builds; root.img is the seed.
.PHONY: disk test-storage
disk: build/root.img
	$(PYTHON) -c 'from pathlib import Path; import sys; p=Path(sys.argv[1]); p.exists() or p.write_bytes(Path("build/root.img").read_bytes())' "$(DISK_IMAGE)"
build/ide.o: kernel/ide.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/ide.o
build/nexix.elf: build/ide.o
-include build/ide.d
test-storage: iso
	$(PYTHON) tools/storage_smoke.py $(QEMU) build/nexix.iso 16M
	$(PYTHON) tools/storage_smoke.py $(QEMU) build/nexix.iso 128M
test: test-storage
build/ide-test: tests/ide.c tests/ide/io.h kernel/ide.c kernel/block.c kernel/include/ide.h kernel/include/block.h include/nexix/errno.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Itests/ide -Ikernel/include -Iinclude tests/ide.c kernel/ide.c kernel/block.c -o $@
test-unit: build/ide-test

# Task 21: read-only source inventory and coverage checks.
.PHONY: test-inventory
test-inventory:
	$(PYTHON) tools/check_inventory.py
	$(PYTHON) tests/inventory.py
	$(PYTHON) tests/roadmap.py
test: test-inventory

# Task 22: boot RTC and shared epoch clock.
build/rtc.o: kernel/arch/x86_64/rtc.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/clock.o: kernel/clock.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/rtc.o build/clock.o
build/nexix.elf: build/rtc.o build/clock.o
-include build/rtc.d build/clock.d
build/rtc-test: tests/rtc.c tests/rtc/io.h kernel/arch/x86_64/rtc.c kernel/clock.c kernel/include/clock.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Itests/rtc -Ikernel/include -Iinclude tests/rtc.c kernel/arch/x86_64/rtc.c kernel/clock.c -o $@
.PHONY: test-rtc
test-rtc: build/rtc-test iso
	./build/rtc-test
	$(PYTHON) tools/rtc_smoke.py $(QEMU) build/nexix.iso
test: test-rtc

# Task 24: test native time code against fixed calendar/DST expectations.
TIME_TEST_NAMES = -Derrno=nx_test_errno -Dgetenv=nx_test_getenv -Dtime=nx_test_time -Dgmtime=nx_test_gmtime -Dlocaltime=nx_test_localtime -Dctime=nx_test_ctime -Dasctime=nx_test_asctime -Dtimezone=nx_test_timezone -Dftime=nx_test_ftime
build/time-test: tests/time.c libc/time.c libc/include/time.h libc/include/sys/timeb.h libc/include/assert.h libc/include/stdlib.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin $(TIME_TEST_NAMES) -Ilibc/include -Iinclude tests/time.c libc/time.c -o $@
.PHONY: test-time
test-time: build/time-test iso
	./build/time-test
	$(PYTHON) tools/time_smoke.py $(QEMU) build/nexix.iso 16M
	$(PYTHON) tools/time_smoke.py $(QEMU) build/nexix.iso 128M
test: test-time

# Task 25: eager native x87/MMX/SSE state management.
build/fpu.o: kernel/arch/x86_64/fpu.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/fpu.o
build/nexix.elf: build/fpu.o
-include build/fpu.d
build/fpu_program.o: user/fpu-test.S include/nexix/syscall.h | build/.dir
	$(CC) $(ASFLAGS) -Iinclude -c $< -o $@
LIBM_OBJECTS = $(patsubst libm/v7/%.c,build/libm_%.o,$(wildcard libm/v7/*.c))
-include $(LIBM_OBJECTS:.o=.d)
build/libm_%.o: libm/v7/%.c | build/.dir
	$(CC) $(V7_CFLAGS) -c $< -o $@
build/libm.a: $(LIBM_OBJECTS)
	$(AR) rcs $@ $^
userland: build/libm.a
.PHONY: test-fpu
test-fpu: test-signals
build/libc_scan_program.o: user/libc-scan-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/libc_scan_program.d
build/stdio_program.o: user/stdio-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/stdio_program.d
.PHONY: test-stdio
test-stdio: test-signals

# Task 38: complete original kernel input/syscall responsibility audit.
.PHONY: test-kernel-coverage
test-kernel-coverage:
	$(PYTHON) tools/check_kernel_coverage.py
	$(PYTHON) tests/kernel_coverage.py
test-inventory: test-kernel-coverage

# Task 39: compile public headers independently of host libc and check ABI records.
build/header-abi-test: tests/header_abi.c $(wildcard libc/include/*.h libc/include/sys/*.h include/nexix/*.h) kernel/include/v7/types.h | build/.dir
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -ffreestanding -nostdinc -isystem $(shell $(HOST_CC) -print-file-name=include) -Ilibc/include -Iinclude -Ikernel/include $< -o $@
.PHONY: test-header-abi
test-header-abi: build/header-abi-test
	./build/header-abi-test
	$(PYTHON) tests/header_coverage.py --cc $(CC)
test-inventory: test-header-abi

# Task 40: native byte/conversion, abort/assert and execle contracts.
build/libc_helpers_program.o: user/libc-helpers-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/libc_helpers_program.d
.PHONY: test-libc-helpers
test-libc-helpers: test-libc
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso libc 128M
.PHONY: test-libc-interfaces
test-libc-interfaces: build/libc.a build/libm.a
	$(PYTHON) tools/check_libc_interfaces.py --cc $(CC) --nm $(TOOLCHAIN_PREFIX)nm
test-libc-helpers: test-libc-interfaces
test: test-libc-interfaces

# Task 41: source adapters and native remote printf packets.
V7_STDIO_ADAPTER_INPUTS = reference/v7/usr/src/cmd/refer/hunt5.c reference/v7/usr/src/cmd/refer/inv1.c reference/v7/usr/src/cmd/refer/what3.c reference/v7/usr/src/cmd/cpp/cpp.c reference/v7/usr/src/games/quiz.c
build/v7-stdio/test-fragments.c: tools/adapt_v7_stdio.py $(V7_STDIO_ADAPTER_INPUTS) | build/.dir
	$(PYTHON) tools/adapt_v7_stdio.py --output build/v7-stdio
build/v7_stdio_fragments.o: build/v7-stdio/test-fragments.c
	$(CC) $(V7_CFLAGS) -c $< -o $@
.PHONY: test-stdio-adapters
test-stdio-adapters:
	$(PYTHON) tests/stdio_adapters.py
test-inventory: test-stdio-adapters
.PHONY: test-stdio-compat
test-stdio-compat: test-stdio-adapters test-stdio test-libc-interfaces

build/stdio_compat_program.o: user/stdio-compat-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/stdio_compat_program.d build/v7_stdio_fragments.d

# Task 42: historical temporary-file naming, host edge cases and native files.
build/mktemp_program.o: user/mktemp-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/mktemp_program.d
build/mktemp-unit-test: tests/mktemp.c libc/mktemp.c libc/include/stdlib.h libc/include/unistd.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin -Dmktemp=nx_test_mktemp -Dgetpid=nx_test_getpid -Daccess=nx_test_access -Derrno=nx_test_errno -Ilibc/include -Iinclude tests/mktemp.c libc/mktemp.c -o $@
.PHONY: test-mktemp
test-mktemp: test-mktemp-unit test-stdio test-libc-interfaces
.PHONY: test-mktemp-unit
test-mktemp-unit: build/mktemp-unit-test
	./build/mktemp-unit-test
test-unit: test-mktemp-unit

# Task 43: historical messages, direct descriptor diagnostics and native errors.
build/perror_program.o: user/perror-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/perror_program.d
build/perror-unit-test: tests/perror.c libc/perror.c libc/errors.c libc/include/stdio.h libc/include/errno.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin -Dperror=nx_test_perror -Dwrite=nx_test_write -Derrno=nx_test_errno -Dsys_errlist=nx_test_errlist -Dsys_nerr=nx_test_nerr -Ilibc/include -Iinclude tests/perror.c libc/perror.c libc/errors.c -o $@
.PHONY: test-perror-unit test-perror
test-perror-unit: build/perror-unit-test
	./build/perror-unit-test
	$(PYTHON) tests/error_table.py
test-perror: test-perror-unit test-stdio test-libc-interfaces
test-unit: test-perror-unit

# Task 44: V7 password-file iteration, lookup and legacy raw-line interface.
build/passwd_program.o: user/passwd-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/passwd_program.d
.PHONY: test-passwd
test-passwd: test-stdio test-libc-interfaces
PASSWD_TEST_NAMES = -Derrno=nx_test_errno -Dfopen=nx_test_fopen -Dfclose=nx_test_fclose -Drewind=nx_test_rewind -Dferror=nx_test_ferror -Dgetc=nx_test_getc -Dgetpwent=nx_test_getpwent -Dsetpwent=nx_test_setpwent -Dendpwent=nx_test_endpwent -Dgetpwnam=nx_test_getpwnam -Dgetpwuid=nx_test_getpwuid -Dgetpw=nx_test_getpw
build/passwd-unit-test: tests/passwd.c libc/passwd.c libc/include/pwd.h libc/include/stdio.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin $(PASSWD_TEST_NAMES) -Ilibc/include -Iinclude tests/passwd.c libc/passwd.c -o $@
.PHONY: test-passwd-unit
test-passwd-unit: build/passwd-unit-test
	./build/passwd-unit-test
test-passwd: test-passwd-unit
test-unit: test-passwd-unit

# Task 45: V7 group-file iteration, lookups and bounded membership vectors.
build/group_program.o: user/group-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/group_program.d
GROUP_TEST_NAMES = -Derrno=nx_test_errno -Dfopen=nx_test_fopen -Dfclose=nx_test_fclose -Drewind=nx_test_rewind -Dferror=nx_test_ferror -Dgetc=nx_test_getc -Dgetgrent=nx_test_getgrent -Dsetgrent=nx_test_setgrent -Dendgrent=nx_test_endgrent -Dgetgrnam=nx_test_getgrnam -Dgetgrgid=nx_test_getgrgid
build/group-unit-test: tests/group.c libc/group.c libc/include/grp.h libc/include/stdio.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin $(GROUP_TEST_NAMES) -Ilibc/include -Iinclude tests/group.c libc/group.c -o $@
.PHONY: test-group-unit test-group
test-group-unit: build/group-unit-test
	./build/group-unit-test
test-group: test-group-unit test-stdio test-libc-interfaces
test-unit: test-group-unit

# Task 46: source-adapted DES and V7 password hashing.
build/crypt_program.o: user/crypt-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/crypt_program.d
CRYPT_TEST_NAMES = -Dcrypt=nx_test_crypt -Dsetkey=nx_test_setkey -Dencrypt=nx_test_encrypt -Derrno=nx_test_errno
build/crypt-unit-test: user/crypt-test.c tests/crypt.c libc/crypt.c libc/include/crypt.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin $(CRYPT_TEST_NAMES) -Ilibc/include -Iinclude user/crypt-test.c tests/crypt.c libc/crypt.c -o $@
build/crypt-oracle.so: libc/crypt.c tests/crypt_errno.c libc/include/crypt.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin -fPIC -shared $(CRYPT_TEST_NAMES) -Ilibc/include -Iinclude libc/crypt.c tests/crypt_errno.c -o $@
.PHONY: test-crypt-unit test-crypt
test-crypt-unit: build/crypt-unit-test build/crypt-oracle.so
	./build/crypt-unit-test
	$(PYTHON) tests/crypt_oracle.py
	$(PYTHON) tests/crypt_tables.py
test-crypt: test-crypt-unit test-libc-helpers test-libc-interfaces
test-unit: test-crypt-unit

# Task 47: terminal/account lookups and password terminal state.
build/terminal_account_program.o: user/terminal-account-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/terminal_account_program.d
.PHONY: test-terminal
test-terminal: test-stdio test-tty test-libc-interfaces
TERMINAL_TEST_NAMES = -Derrno=nx_test_errno -Dopen=nx_test_open -Dclose=nx_test_close -Dread=nx_test_read -Dlseek=nx_test_lseek -Dstat=nx_test_stat -Dfstat=nx_test_fstat -Disatty=nx_test_isatty -Dttyname=nx_test_ttyname -Dttyslot=nx_test_ttyslot -Dgetlogin=nx_test_getlogin
build/terminal-unit-test: tests/terminal.c libc/terminal.c libc/include/unistd.h libc/include/utmp.h libc/include/sys/dir.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin $(TERMINAL_TEST_NAMES) -Ilibc/include -Iinclude tests/terminal.c libc/terminal.c -o $@
GETPASS_TEST_NAMES = -Derrno=nx_test_errno -Dgetpass=nx_test_getpass -Dfopen=nx_test_fopen -Dfclose=nx_test_fclose -Dsetbuf=nx_test_setbuf -Dfileno=nx_test_fileno -Dsignal=nx_test_signal -Dgtty=nx_test_gtty -Dstty=nx_test_stty -Dfputs=nx_test_fputs -Dfflush=nx_test_fflush -Dgetc=nx_test_getc -Dferror=nx_test_ferror -Dstdin=nx_test_stdin -Dstderr=nx_test_stderr
build/getpass-unit-test: tests/getpass.c libc/getpass.c libc/include/unistd.h libc/include/sgtty.h libc/include/signal.h libc/include/stdio.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin $(GETPASS_TEST_NAMES) -Ilibc/include -Iinclude tests/getpass.c libc/getpass.c -o $@
.PHONY: test-terminal-unit
test-terminal-unit: build/terminal-unit-test build/getpass-unit-test
	./build/terminal-unit-test
	./build/getpass-unit-test
test-terminal: test-terminal-unit
test-unit: test-terminal-unit

# Task 48: V7 shell execution, wait status and signal dispositions.
build/system_program.o: user/system-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/system_program.d
SYSTEM_TEST_NAMES = -Dsystem=nx_test_system -Derrno=nx_test_errno -Dfork=nx_test_fork -Dsignal=nx_test_signal -Dwait=nx_test_wait -Dexecl=nx_test_execl -D_exit=nx_test_exit
build/system-unit-test: tests/system.c libc/system.c libc/include/stdlib.h libc/include/signal.h libc/include/sys/wait.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin $(SYSTEM_TEST_NAMES) -Ilibc/include -Iinclude tests/system.c libc/system.c -o $@
.PHONY: test-system-unit test-system
test-system-unit: build/system-unit-test
	./build/system-unit-test
	./build/system-unit-test child
test-system: test-system-unit test-v7 test-libc-interfaces
test-unit: test-system-unit

# Task 49: directional process pipes and matching child status.
build/popen_program.o: user/popen-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/popen_program.d
.PHONY: test-popen
test-popen: test-v7 test-libc-interfaces
POPEN_TEST_NAMES = -Dpopen=nx_test_popen -Dpclose=nx_test_pclose -Derrno=nx_test_errno -Dpipe=nx_test_pipe -Dclose=nx_test_close -Dfdopen=nx_test_fdopen -Dfclose=nx_test_fclose -Dfork=nx_test_fork -Ddup2=nx_test_dup2 -Dexecl=nx_test_execl -D_exit=nx_test_exit -Dsignal=nx_test_signal -Dwait=nx_test_wait
build/popen-unit-test: tests/popen.c libc/popen.c libc/include/stdio.h libc/include/sys/wait.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin $(POPEN_TEST_NAMES) -Ilibc/include -Iinclude tests/popen.c libc/popen.c -o $@
.PHONY: test-popen-unit
test-popen-unit: build/popen-unit-test
	./build/popen-unit-test
	./build/popen-unit-test read-child
	./build/popen-unit-test write-child
	./build/popen-unit-test dup-fail
test-popen: test-popen-unit
test-unit: test-popen-unit

# Task 50: staged V7 set-ID credentials and real filesystem exec acceptance.
build/setid_program.o: user/setid-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/setid_probe_program.o: user/setid-probe.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/setid-probe.elf: build/crt0.o build/setid_probe_program.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/setid_probe_program.o build/libc.a -o $@
build/setid_probe_image.o: build/setid-probe.elf
	$(LD) -r -b binary $< -o build/setid_probe_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/setid_probe_raw.o $@
OBJECTS += build/setid_probe_image.o
build/nexix.elf: build/setid_probe_image.o
-include build/setid_program.d build/setid_probe_program.d
.PHONY: test-setid
test-setid: test-rootfs test-inventory test-libc-interfaces

# Task 51: resident executable text/inode ownership.
build/text.o: kernel/v7/text.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/text.o
build/nexix.elf: build/text.o
-include build/text.d
.PHONY: test-text
test-text: test-rootfs test-inventory test-libc-interfaces
build/text-unit-test: tests/text.c kernel/v7/text.c kernel/include/text.h kernel/include/v7/core.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/text.c kernel/v7/text.c -o $@
.PHONY: test-text-unit
test-text-unit: build/text-unit-test
	./build/text-unit-test
test-unit: test-text-unit
test-text: test-text-unit

build/text_program.o: user/text-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/text_probe_program.o: user/text-probe.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/text-probe.elf: build/crt0.o build/text_probe_program.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/text_probe_program.o build/libc.a -o $@
build/text_probe_image.o: build/text-probe.elf
	$(LD) -r -b binary $< -o build/text_probe_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/text_probe_raw.o $@
OBJECTS += build/text_probe_image.o
build/nexix.elf: build/text_probe_image.o
-include build/text_program.d build/text_probe_program.d
# Task 52: V7 fatal-signal policy and native ELF64 core images.
build/core.o: kernel/v7/core.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/core.o
build/nexix.elf: build/core.o
-include build/core.d
.PHONY: test-core
test-core: test-rootfs test-inventory test-libc-interfaces
build/core-unit-test: tests/core.c kernel/v7/core.c kernel/include/core.h include/nexix/core.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/core.c kernel/v7/core.c -o $@
.PHONY: test-core-unit
test-core-unit: build/core-unit-test
	./build/core-unit-test
	$(PYTHON) tests/core_format.py
test-core: test-core-unit
test-unit: test-core-unit

build/core_program.o: user/core-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/core_fault.o: user/core-fault.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
build/core_probe_program.o: user/core-probe.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/core-probe.elf: build/crt0.o build/core_probe_program.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/core_probe_program.o build/libc.a -o $@
build/core_probe_image.o: build/core-probe.elf
	$(LD) -r -b binary $< -o build/core_probe_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/core_probe_raw.o $@
OBJECTS += build/core_probe_image.o
build/nexix.elf: build/core_probe_image.o
-include build/core_program.d build/core_probe_program.d
