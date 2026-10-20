# Host GCC/binutils are the default bootstrap tools on x86_64 Linux.
# Override with TOOLCHAIN_PREFIX=x86_64-elf- for a cross-toolchain.
TOOLCHAIN_PREFIX ?=
CC = $(TOOLCHAIN_PREFIX)gcc
LD = $(TOOLCHAIN_PREFIX)ld
AR = $(TOOLCHAIN_PREFIX)ar
OBJCOPY = $(TOOLCHAIN_PREFIX)objcopy
PYTHON ?= python3
QEMU ?= qemu-system-x86_64
CONSOLE ?= vga
DISK_IMAGE ?= build/nexix.img
INSTALL_MANIFEST ?= docs/native-install.json
INSTALL_CONFIG ?= rootfs
INSTALL_STAGE ?= build/stage
FULL_ROOT_IMAGE ?= build/distribution.img
CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -fno-pie -m64 -mno-red-zone -mgeneral-regs-only -fno-asynchronous-unwind-tables -Ikernel/include -Iinclude -MMD -MP
OBJECTS = build/klog.o build/console.o build/packet.o build/packet_tty.o build/packet_image.o build/boot.o build/main.o build/interrupts.o build/interrupt_stubs.o build/boot_info.o build/memory.o build/paging.o build/paging_test.o build/segments.o build/user_entry.o build/user_blob.o build/user_test.o build/process.o build/context.o build/process_test.o build/process_blob.o build/uaccess.o build/syscall.o build/syscall_test.o build/syscall_blob.o build/elf.o build/exec.o build/exec_test.o build/exec_image.o build/libc_test.o build/libc_image.o build/block.o build/v7fs.o build/fs_test.o build/root_image.o build/files.o build/files_test.o build/files_image.o build/tty.o build/tty_test.o build/tty_image.o build/signals.o build/signals_test.o build/signals_image.o build/catalog.o build/v7_userland_test.o build/v7_test_image.o
include build/native-config.mk
OBJECTS += $(NATIVE_CONFIG_OBJECTS)

build/native-config.mk: config/kernel.json tools/mkconf.py
	$(PYTHON) tools/mkconf.py config/kernel.json $@

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

build/serial_secondary.o: kernel/serial_secondary.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/klog.o: kernel/klog.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/console.o: kernel/console.c | build/.dir
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

build/nexix.iso: build/nexix.elf boot/grub.cfg tools/pack_kernel.py
	mkdir -p build/iso/boot/grub
	$(PYTHON) tools/pack_kernel.py build/nexix.elf build/iso/boot/nexix.elf
	cp boot/grub.cfg build/iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ build/iso

iso: check build/nexix.iso

serial-iso: check build/nexix-serial.iso
multi-iso: check build/nexix-multi.iso
build: iso disk
run-serial: build serial-iso
	$(PYTHON) tools/serial_terminal.py --prepare build/ttyS1
	$(QEMU) -machine pc -accel tcg -cpu qemu64 -smp 1 -m 128M -rtc base=utc -boot d -drive "file=$(DISK_IMAGE),format=raw,if=ide,index=0" -cdrom build/nexix-serial.iso -display none -chardev stdio,id=console,mux=on,signal=off -serial chardev:console -serial pipe:build/ttyS1 -mon chardev=console,mode=readline -no-reboot

.PHONY: terminal
terminal:
	$(PYTHON) tools/serial_terminal.py build/ttyS1

smoke: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso

build/test-%.iso: build/nexix.elf boot/grub.cfg tools/pack_kernel.py
	mkdir -p build/iso-$*/boot/grub
	$(PYTHON) tools/pack_kernel.py build/nexix.elf build/iso-$*/boot/nexix.elf
	sed -e 's|set nexix_console=vga|set nexix_console=multi|' -e 's|/boot/nexix.elf|/boot/nexix.elf test=$*|' boot/grub.cfg > build/iso-$*/boot/grub/grub.cfg
	grub-mkrescue -o $@ build/iso-$*

build/test-memory.iso: build/nexix.elf boot/grub.cfg tools/pack_kernel.py
	mkdir -p build/iso-memory/boot/grub
	$(PYTHON) tools/pack_kernel.py build/nexix.elf build/iso-memory/boot/nexix.elf
	$(PYTHON) -c 'from pathlib import Path; Path("build/iso-memory/boot/reservation.bin").write_bytes(bytes([0xa5]) * 8193)'
	sed -e 's|set nexix_console=vga|set nexix_console=multi|' -e 's|/boot/nexix.elf|/boot/nexix.elf test=memory|' boot/grub.cfg | sed '/multiboot2/a\    module2 /boot/reservation.bin allocator-test' > build/iso-memory/boot/grub/grub.cfg
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
build/exec_image.o: build/exec-test.elf tools/embed_test_elf.py
	$(PYTHON) tools/embed_test_elf.py $< $@ --ld $(LD) --objcopy $(OBJCOPY)
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
LIBC_OBJECTS = build/libc_packet.o build/libc_syscall.o build/libc_syscalls.o build/libc_string.o build/libc_stdlib.o build/libc_signal.o build/libc_setjmp.o build/libc_stdio.o build/libc_malloc.o build/libc_exec.o build/libc_time.o build/libc_sleep.o build/libc_float.o build/libc_convert.o build/libc_scan.o build/libc_ctype.o build/libc_numbers.o build/libc_bytes.o build/libc_gcvt.o build/libc_abort.o build/libc_mktemp.o build/libc_errors.o build/libc_perror.o build/libc_passwd.o build/libc_group.o build/libc_crypt.o build/libc_terminal.o build/libc_getpass.o build/libc_system.o build/libc_popen.o build/libc_ptrace.o build/libc_profil.o build/libc_monitor.o build/libc_mcount.o build/libc_nlist.o build/libc_kinfo.o build/libc_uname.o build/libc_lock.o build/libc_phys.o build/libc_mpx.o
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
build/libc_image.o: build/libc-test.elf tools/embed_test_elf.py
	$(PYTHON) tools/embed_test_elf.py $< $@ --ld $(LD) --objcopy $(OBJCOPY)
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
build/files_image.o: build/files-test.elf tools/embed_test_elf.py
	$(PYTHON) tools/embed_test_elf.py $< $@ --ld $(LD) --objcopy $(OBJCOPY)
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
build/tty_image.o: build/tty-test.elf tools/embed_test_elf.py
	$(PYTHON) tools/embed_test_elf.py $< $@ --ld $(LD) --objcopy $(OBJCOPY)
userland: build/tty-test.elf

.PHONY: test-tty run-tty
test-tty: check build/test-tty.iso build/test-tty-demo.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-tty.iso tty 16M
	$(PYTHON) tools/smoke.py $(QEMU) build/test-tty.iso tty 128M
	$(PYTHON) tools/smoke.py $(QEMU) build/test-tty-demo.iso tty-demo 16M
run-tty: check build/test-tty-demo.iso
	$(QEMU) -machine pc -accel tcg -cpu qemu64 -smp 1 -m 128M -cdrom build/test-tty-demo.iso -display none -chardev stdio,id=console,mux=on,signal=off -serial chardev:console -mon chardev=console,mode=readline -no-reboot

build/tty-unit-test: kernel/v7/tty_controller.c kernel/include/tty_controller.h tests/packet-inactive.c tests/tty.c kernel/v7/tty.c kernel/include/tty.h kernel/include/serial.h kernel/include/uaccess.h kernel/include/process.h kernel/include/v7/core.h include/nexix/tty.h include/nexix/errno.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/tty.c tests/packet-inactive.c kernel/v7/tty.c kernel/v7/tty_controller.c -o $@

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
build/signals_image.o: build/signals-test.elf tools/embed_test_elf.py
	$(PYTHON) tools/embed_test_elf.py $< $@ --ld $(LD) --objcopy $(OBJCOPY)
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
INSTALL_COMMANDS := $(shell $(PYTHON) tools/install_manifest.py --manifest $(INSTALL_MANIFEST) --commands)
INSTALL_INPUTS := $(shell $(PYTHON) tools/install_manifest.py --manifest $(INSTALL_MANIFEST) --config $(INSTALL_CONFIG) --inputs)
BOOTSTRAP_INPUTS := $(shell $(PYTHON) tools/install_manifest.py --manifest docs/bootstrap-install.json --config rootfs/bootstrap --inputs)
INSTALL_EMBEDDED := $(shell $(PYTHON) tools/install_manifest.py --manifest docs/bootstrap-install.json --embedded)
ifeq ($(strip $(INSTALL_COMMANDS)),)
$(error Invalid or empty native installation manifest; see tools/install_manifest.py)
endif
V7_COMMANDS = $(filter-out sh,$(INSTALL_COMMANDS))
V7_SH_SOURCES = $(wildcard user/v7/sh/*.c)
V7_SH_OBJECTS = $(patsubst user/v7/sh/%.c,build/v7-sh-%.o,$(V7_SH_SOURCES))
V7_ELFS = $(filter build/%.elf,$(INSTALL_INPUTS))
-include $(V7_SH_OBJECTS:.o=.d) $(addprefix build/v7-,$(addsuffix .d,$(V7_COMMANDS)))
build/v7-sh-%.o: user/v7/sh/%.c | build/.dir
	$(CC) $(V7_CFLAGS) -c $< -o $@
build/v7-%.o: user/v7/cmd/%.c user/v7/compat.h | build/.dir
	$(CC) $(V7_CFLAGS) -include user/v7/compat.h -c $< -o $@
build/v7-sh.elf: build/crt0.o $(V7_SH_OBJECTS) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o $(V7_SH_OBJECTS) build/libc.a -o $@
build/v7-%.elf: build/crt0.o build/v7-%.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o,$^) $(filter %.a,$^) -o $@
build/v7-init.elf build/v7-login.elf: build/v7-session.o
-include build/v7-session.d
build/v7-session.o: user/v7/session.c user/v7/session.h | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
.PHONY: v7-userland
v7-userland: $(V7_ELFS)
userland: v7-userland

V7_IMAGES = $(addprefix build/v7_image_,$(addsuffix .o,$(INSTALL_EMBEDDED)))
# Embedded executable images for the task 16 acceptance fixture.
OBJECTS += $(V7_IMAGES)
-include build/catalog.d build/v7_userland_test.d build/v7_test_program.d
build/install_catalog.inc: docs/bootstrap-install.json docs/v7-userland-manifest.json tools/install_manifest.py | build/.dir
	$(PYTHON) tools/install_manifest.py --manifest docs/bootstrap-install.json --catalog > $@
build/catalog.o: kernel/v7/catalog.c build/install_catalog.inc | build/.dir
	$(CC) $(CFLAGS) -Ibuild -c $< -o $@
build/v7_userland_test.o: kernel/v7_userland_test.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/v7_image_%.o: build/v7-%.elf
	$(LD) -r -b binary $< -o build/v7_raw_$*.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/v7_raw_$*.o $@
build/v7_test_program.o: user/v7-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/v7-regression.elf: build/crt0.o build/v7_test_program.o build/system_program.o build/popen_program.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/v7_test_program.o build/system_program.o build/popen_program.o build/libc.a -o $@
build/v7_test_image.o: build/v7-regression.elf tools/embed_test_elf.py
	$(PYTHON) tools/embed_test_elf.py $< $@ --ld $(LD) --objcopy $(OBJCOPY)
.PHONY: test-v7
test-v7: check build/test-selftest.iso
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso v7 16M
	$(PYTHON) tools/smoke.py $(QEMU) build/test-selftest.iso v7 128M
build/nexix.elf: $(V7_IMAGES)

.SECONDARY: $(addprefix build/v7-,$(addsuffix .o,$(V7_COMMANDS)))

# Packaged 0.1.0 root, separate from the filesystem regression fixture.
.PHONY: rootfs bootstrap-rootfs test-rootfs stage bootstrap-stage
stage: $(INSTALL_STAGE)/receipt.json
	$(PYTHON) tools/install_manifest.py --manifest $(INSTALL_MANIFEST) --verify --output $(INSTALL_STAGE)
$(INSTALL_STAGE)/receipt.json: $(INSTALL_MANIFEST) docs/v7-userland-manifest.json tools/install_manifest.py $(INSTALL_INPUTS) | build/.dir
	$(PYTHON) tools/install_manifest.py --manifest $(INSTALL_MANIFEST) --config $(INSTALL_CONFIG) --output $(INSTALL_STAGE)
rootfs: $(FULL_ROOT_IMAGE)
$(FULL_ROOT_IMAGE): tools/mk_rootfs.py tools/mk_v7fs.py tools/install_manifest.py $(INSTALL_STAGE)/receipt.json | stage
	$(PYTHON) tools/mk_rootfs.py --manifest $(INSTALL_MANIFEST) --stage $(INSTALL_STAGE) --blocks auto --spare-blocks 32768 --spare-inodes 2048 --output $@
bootstrap-stage: build/bootstrap-stage/receipt.json
	$(PYTHON) tools/install_manifest.py --manifest docs/bootstrap-install.json --verify --output build/bootstrap-stage
build/bootstrap-stage/receipt.json: docs/bootstrap-install.json docs/v7-userland-manifest.json tools/install_manifest.py $(BOOTSTRAP_INPUTS) | build/.dir
	$(PYTHON) tools/install_manifest.py --manifest docs/bootstrap-install.json --config rootfs/bootstrap --output build/bootstrap-stage
bootstrap-rootfs: build/root.img
build/root.img: tools/mk_rootfs.py tools/mk_v7fs.py tools/install_manifest.py build/bootstrap-stage/receipt.json | bootstrap-stage
	$(PYTHON) tools/mk_rootfs.py --manifest docs/bootstrap-install.json --stage build/bootstrap-stage --blocks 1024 --inodes 96 --output $@
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
build/root-exec-test.elf: build/crt0.o build/root_exec_program.o build/memdev_program.o build/disk_program.o build/disk_order_program.o build/tape_program.o build/dectape_program.o build/setid_program.o build/text_program.o build/core_program.o build/core_fault.o build/storage_program.o build/namespace_program.o build/umount_program.o build/accounting_program.o build/ptrace_program.o build/ptrace_probe.o build/debugger_program.o build/profile_program.o build/profile_work.o build/monitor_program.o build/monitor_abi.o build/nlist_program.o build/kinfo_program.o build/lock_program.o build/phys_program.o build/mpx_program.o build/mpx_data_program.o build/mpx_tty_program.o build/tty_controller_program.o build/modem_program.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/root_exec_program.o build/memdev_program.o build/disk_program.o build/disk_order_program.o build/tape_program.o build/dectape_program.o build/setid_program.o build/text_program.o build/core_program.o build/core_fault.o build/storage_program.o build/namespace_program.o build/umount_program.o build/accounting_program.o build/ptrace_program.o build/ptrace_probe.o build/debugger_program.o build/profile_program.o build/profile_work.o build/monitor_program.o build/monitor_abi.o build/nlist_program.o build/kinfo_program.o build/lock_program.o build/phys_program.o build/mpx_program.o build/mpx_data_program.o build/mpx_tty_program.o build/tty_controller_program.o build/modem_program.o build/libc.a -o $@
build/root_exec_image.o: build/root-exec-test.elf tools/embed_test_elf.py
	$(PYTHON) tools/embed_test_elf.py $< $@ --ld $(LD) --objcopy $(OBJCOPY)
OBJECTS += build/root_exec_image.o
build/nexix.elf: build/root_exec_image.o
-include build/root_exec_program.d
.PHONY: test-boot
test-boot: multi-iso
	$(PYTHON) tools/boot_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/boot_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-boot

.PHONY: test-shell
test-shell: multi-iso rootfs
	$(PYTHON) tools/interactive_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/interactive_smoke.py $(QEMU) build/nexix-multi.iso 128M
	$(PYTHON) tools/interactive_smoke.py $(QEMU) build/nexix-multi.iso 16M ide
	$(PYTHON) tools/interactive_smoke.py $(QEMU) build/nexix-multi.iso 128M ide
test: test-shell

# Preserve working disks; only new disks receive the full distribution seed.
.PHONY: disk test-storage
disk: $(FULL_ROOT_IMAGE) tools/seed_disk.py
	$(PYTHON) tools/seed_disk.py --seed "$(FULL_ROOT_IMAGE)" --output "$(DISK_IMAGE)"
build/ide.o: kernel/ide.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/nexix.elf: build/ide.o
-include build/ide.d
test-storage: rootfs multi-iso
	$(PYTHON) tools/storage_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/storage_smoke.py $(QEMU) build/nexix-multi.iso 128M
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
OBJECTS += build/clock.o
build/nexix.elf: build/rtc.o build/clock.o
-include build/rtc.d build/clock.d
build/rtc-test: tests/rtc.c tests/rtc/io.h kernel/arch/x86_64/rtc.c kernel/clock.c kernel/include/clock.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Itests/rtc -Ikernel/include -Iinclude tests/rtc.c kernel/arch/x86_64/rtc.c kernel/clock.c -o $@
.PHONY: test-rtc
test-rtc: build/rtc-test multi-iso
	./build/rtc-test
	$(PYTHON) tools/rtc_smoke.py $(QEMU) build/nexix-multi.iso
test: test-rtc

# Task 24: test native time code against fixed calendar/DST expectations.
TIME_TEST_NAMES = -Derrno=nx_test_errno -Dgetenv=nx_test_getenv -Dtime=nx_test_time -Dgmtime=nx_test_gmtime -Dlocaltime=nx_test_localtime -Dctime=nx_test_ctime -Dasctime=nx_test_asctime -Dtimezone=nx_test_timezone -Dftime=nx_test_ftime
build/time-test: tests/time.c libc/time.c libc/include/time.h libc/include/sys/timeb.h libc/include/assert.h libc/include/stdlib.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin $(TIME_TEST_NAMES) -Ilibc/include -Iinclude tests/time.c libc/time.c -o $@
.PHONY: test-time
test-time: build/time-test multi-iso
	./build/time-test
	$(PYTHON) tools/time_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/time_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-time

# Task 25: eager native x87/MMX/SSE state management.
build/fpu.o: kernel/arch/x86_64/fpu.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

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
build/setid_probe_image.o: build/setid-probe.elf tools/embed_test_elf.py
	$(PYTHON) tools/embed_test_elf.py $< $@ --ld $(LD) --objcopy $(OBJCOPY)
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

OBJECTS += build/storage.o
build/nexix.elf: build/storage.o
-include build/storage.d build/storage_program.d
build/storage.o: kernel/v7/storage.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/storage_program.o: user/storage-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/storage-unit-test: tests/storage.c kernel/v7/storage.c kernel/v7/disk.c kernel/v7/disk_order.c kernel/v7/dectape.c kernel/block.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $^ -o $@
.PHONY: test-storage test-storage-unit
test-storage-unit: build/storage-unit-test
	./build/storage-unit-test
test-storage: test-storage-unit test-rootfs test-inventory test-libc-interfaces
test-unit: test-storage-unit

OBJECTS += build/namespace.o
build/nexix.elf: build/namespace.o
-include build/namespace.d
build/namespace.o: kernel/v7/namespace.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/namespace.img: tools/mk_namespace_fixture.py tools/mk_v7fs.py build/v7-echo.elf build/text-probe.elf | build/.dir
	$(PYTHON) tools/mk_namespace_fixture.py
build/namespace_image.o: build/namespace.img
	$(LD) -r -b binary $< -o build/namespace_image_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/namespace_image_raw.o $@
OBJECTS += build/namespace_image.o
build/nexix.elf: build/namespace_image.o

build/namespace_program.o: user/namespace-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/namespace_program.d

build/namespace-unit-test: tests/namespace.c kernel/v7/namespace.c kernel/v7/storage.c kernel/v7/disk.c kernel/v7/disk_order.c kernel/v7/dectape.c kernel/block.c kernel/v7/fs.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $^ -o $@
.PHONY: test-mount test-namespace-unit
test-namespace-unit: build/namespace-unit-test build/namespace.img
	./build/namespace-unit-test build/namespace.img
test-mount: test-namespace-unit test-rootfs test-inventory test-libc-interfaces
test-unit: test-namespace-unit

build/umount-unit-test: tests/umount.c kernel/v7/namespace.c kernel/v7/storage.c kernel/v7/disk.c kernel/v7/disk_order.c kernel/v7/dectape.c kernel/block.c kernel/v7/fs.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $^ -o $@
.PHONY: test-umount test-umount-unit
test-umount-unit: build/umount-unit-test build/namespace.img test-text-unit
	./build/umount-unit-test build/namespace.img
test-umount: test-umount-unit test-rootfs test-inventory test-libc-interfaces
test-unit: test-umount-unit

build/umount_program.o: user/umount-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/umount_program.d

OBJECTS += build/accounting.o
build/nexix.elf: build/accounting.o
-include build/accounting.d
build/accounting.o: kernel/v7/accounting.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

build/accounting-unit-test: tests/accounting.c kernel/v7/accounting.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $^ -o $@
build/accounting-fs-test: tests/accounting_fs.c kernel/v7/fs.c kernel/block.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $^ -o $@
.PHONY: test-accounting test-accounting-unit
test-accounting-unit: build/accounting-unit-test build/accounting-fs-test build/namespace.img
	./build/accounting-unit-test
	./build/accounting-fs-test build/namespace.img
test-accounting: test-accounting-unit test-rootfs test-inventory test-libc-interfaces
test-unit: test-accounting-unit

build/accounting_program.o: user/accounting-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/accounting_program.d

# Task 57: stopped-child tracing and native register access.
OBJECTS += build/ptrace.o
build/nexix.elf: build/ptrace.o
-include build/ptrace.d build/ptrace_program.d
build/ptrace.o: kernel/v7/ptrace.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/ptrace_program.o: user/ptrace-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/ptrace_probe.o: user/ptrace-probe.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
.PHONY: test-ptrace
test-ptrace: test-rootfs test-inventory test-libc-interfaces

build/libc_ptrace.o: libc/ptrace.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@

# Task 58 native debugger instruction/data patch fixture.
build/trace_image_program.o: user/trace-image.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/trace_image_code.o: user/trace-image.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
build/trace-image.elf: build/crt0.o build/trace_image_program.o build/trace_image_code.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/trace_image_program.o build/trace_image_code.o build/libc.a -o $@
build/trace_image.o: build/trace-image.elf
	$(LD) -r -b binary $< -o build/trace_image_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/trace_image_raw.o $@
OBJECTS += build/trace_image.o
build/nexix.elf: build/trace_image.o
-include build/trace_image_program.d

build/debugger_program.o: user/debugger-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/debugger_program.d

.PHONY: test-debugger
test-debugger: build/uaccess-test test-rootfs test-inventory test-libc-interfaces
	./build/uaccess-test

# Task 59: V7 profil sampling with native user addresses.
OBJECTS += build/profile.o
build/nexix.elf: build/profile.o
-include build/profile.d build/profile_program.d
build/profile.o: kernel/v7/profile.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/libc_profil.o: libc/profil.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/profile_program.o: user/profile-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/profile_work.o: user/profile-work.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
build/profile-unit-test: tests/profile.c kernel/v7/profile.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $^ -o $@
.PHONY: test-profile test-profile-unit
test-profile-unit: build/profile-unit-test
	./build/profile-unit-test
test-profile: test-profile-unit test-rootfs test-inventory test-libc-interfaces
test-unit: test-profile-unit

# Task 60: native monitor/mcount and profiling startup objects.
build/libc_mcount.o: libc/arch/x86_64/mcount.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
build/mcrt0.o: libc/arch/x86_64/mcrt0.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
build/fmcrt0.o: libc/arch/x86_64/fmcrt0.S libc/arch/x86_64/mcrt0.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
userland: build/mcrt0.o build/fmcrt0.o
MONITOR_TEST_NAMES = -Derrno=nx_test_errno -Dmonitor=nx_test_monitor -Dprofil=nx_test_profil -Dcreat=nx_test_creat -Dwrite=nx_test_write -Dclose=nx_test_close -Dfflush=nx_test_fflush -Dcalloc=nx_test_calloc -Dfree=nx_test_free -Datexit=nx_test_atexit
build/monitor-unit-test: tests/monitor.c libc/monitor.c libc/include/mon.h include/nexix/mon.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin $(MONITOR_TEST_NAMES) -Ilibc/include -Iinclude tests/monitor.c libc/monitor.c -o $@
.PHONY: test-monitor-unit
test-monitor-unit: build/monitor-unit-test
	./build/monitor-unit-test
test-unit: test-monitor-unit

build/monitor_fixture_program.o: user/monitor-fixture.c | build/.dir
	$(CC) $(USER_CFLAGS) -pg -fno-optimize-sibling-calls -c $< -o $@
build/monitor_abi.o: user/monitor-abi.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
build/monitor-fixture.elf: build/mcrt0.o build/monitor_fixture_program.o build/monitor_abi.o build/profile_work.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/mcrt0.o build/monitor_fixture_program.o build/monitor_abi.o build/profile_work.o build/libc.a -o $@
build/fmonitor-fixture.elf: build/fmcrt0.o build/monitor_fixture_program.o build/monitor_abi.o build/profile_work.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/fmcrt0.o build/monitor_fixture_program.o build/monitor_abi.o build/profile_work.o build/libc.a -o $@
build/monitor_fixture.o: build/monitor-fixture.elf
	$(LD) -r -b binary $< -o build/monitor_fixture_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/monitor_fixture_raw.o $@
build/fmonitor_fixture.o: build/fmonitor-fixture.elf
	$(LD) -r -b binary $< -o build/fmonitor_fixture_raw.o
	$(OBJCOPY) --rename-section .data=.rodata,alloc,load,readonly,data,contents build/fmonitor_fixture_raw.o $@
OBJECTS += build/monitor_fixture.o build/fmonitor_fixture.o
build/nexix.elf: build/monitor_fixture.o build/fmonitor_fixture.o
build/monitor_program.o: user/monitor-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/monitor_program.d build/monitor_fixture_program.d
.PHONY: test-monitor
test-monitor: test-monitor-unit test-rootfs test-inventory test-libc-interfaces

# Task 61: native ELF symbol lookup and V7 nlist contract.
NLIST_TEST_NAMES = -Derrno=nx_test_errno -Dnlist=nx_test_nlist -Dopen=nx_test_open -Dclose=nx_test_close -Dfstat=nx_test_fstat -Dlseek=nx_test_lseek -Dread=nx_test_read
build/nlist-unit-test: tests/nlist.c tests/nlist-fixture.h libc/nlist.c libc/include/nlist.h libc/include/elf.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin $(NLIST_TEST_NAMES) -Ilibc/include -Iinclude tests/nlist.c libc/nlist.c -o $@
build/nlist_program.o: user/nlist-test.c tests/nlist-fixture.h | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/nlist_program.d
.PHONY: test-nlist-unit test-nlist
test-nlist-unit: build/nlist-unit-test
	./build/nlist-unit-test
test-unit: test-nlist-unit
test-nlist: test-nlist-unit test-rootfs test-header-abi test-libc-interfaces

# Task 62: versioned native kernel table snapshots.
build/kinfo.o: kernel/v7/kinfo.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/kinfo.o
build/nexix.elf: build/kinfo.o
-include build/kinfo.d
build/kinfo-unit-test: tests/kinfo.c kernel/v7/kinfo.c include/nexix/kinfo.h kernel/include/kinfo.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/kinfo.c kernel/v7/kinfo.c -o $@
.PHONY: test-kinfo-unit test-kinfo
test-kinfo-unit: build/kinfo-unit-test
	./build/kinfo-unit-test
test-unit: test-kinfo-unit
test-kinfo: test-kinfo-unit test-rootfs test-inventory test-libc-interfaces

build/kinfo_program.o: user/kinfo-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/kinfo_program.d
build/kinfo-io-test: tests/kinfo-io.c kernel/block.c kernel/v7/fs.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $^ -o $@
test-kinfo-io-unit: build/kinfo-io-test build/root-test.img
	./build/kinfo-io-test build/root-test.img
.PHONY: test-kinfo-io-unit
test-kinfo-unit: test-kinfo-io-unit

# Task 63: V7 user-settable resident-memory lock.
build/lock.o: kernel/v7/lock.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/lock.o
build/nexix.elf: build/lock.o
-include build/lock.d
build/lock-unit-test: tests/lock.c kernel/v7/lock.c kernel/v7/accounting.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections -Ikernel/include -Iinclude $^ -o $@
.PHONY: test-lock-unit test-lock
test-lock-unit: build/lock-unit-test
	./build/lock-unit-test
test-unit: test-lock-unit
test-lock: test-lock-unit test-rootfs test-inventory test-libc-interfaces

build/lock_program.o: user/lock-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/lock_program.d

# Task 64: owned native physical mappings.
build/phys.o: kernel/v7/phys.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/phys.o
build/nexix.elf: build/phys.o
-include build/phys.d
build/phys-unit-test: tests/phys.c kernel/v7/phys.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $^ -o $@
.PHONY: test-phys-unit test-phys
test-phys-unit: build/phys-unit-test
	./build/phys-unit-test
test-unit: test-phys-unit
test-phys: test-phys-unit test-rootfs test-inventory test-libc-interfaces

build/phys_program.o: user/phys-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/phys_program.d

# Task 65: V7 multiplexor ownership (data/control and TTY bridging: 66/67).
build/mpx_program.o: user/mpx-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/mpx_program.d
.PHONY: test-mpx
test-mpx: test-rootfs test-inventory test-libc-interfaces

# Task 66: multiplexor data/control records, queues and wakeups.
build/mpx_data_program.o: user/mpx-data-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/mpx_data_program.d
.PHONY: test-mpx-data
test-mpx-data: test-rootfs test-inventory test-libc-interfaces

build/mpx-unit-test: kernel/v7/tty_controller.c kernel/include/tty_controller.h tests/packet-inactive.c tests/mpx.c kernel/v7/tty.c kernel/v7/mpx_tty.inc kernel/v7/files.c kernel/v7/mpx.inc kernel/v7/mpx_io.inc include/nexix/mpx.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections -Ikernel/include -Iinclude tests/mpx.c tests/packet-inactive.c kernel/v7/tty.c kernel/v7/tty_controller.c -o $@
.PHONY: test-mpx-unit
test-mpx-unit: build/mpx-unit-test
	./build/mpx-unit-test
test-unit: test-mpx-unit
test-mpx-data: test-mpx-unit

# Task 67: TTY multiplexor bridging and V7 libc wrappers.
build/mpx_tty_program.o: user/mpx-tty-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/mpx_tty_program.d
.PHONY: test-mpx-tty
test-mpx-tty: test-mpx-unit test-rootfs test-inventory test-libc-interfaces

# Task 68: V7 packet line discipline over the COM1 UART.
build/packet.o: kernel/v7/packet.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/packet_tty.o: kernel/v7/packet_tty.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/packet_program.o: user/packet-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/packet_program.d
build/packet-test.elf: build/crt0.o build/packet_program.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/packet_program.o build/libc.a -o $@
build/packet_image.o: build/packet-test.elf tools/embed_test_elf.py
	$(PYTHON) tools/embed_test_elf.py $< $@ --ld $(LD) --objcopy $(OBJCOPY)
.PHONY: test-packet
test-packet: check build/test-packet.iso test-packet-unit test-libc-interfaces test-rootfs test-inventory
	$(PYTHON) tools/packet_smoke.py $(QEMU) build/test-packet.iso 16M
	$(PYTHON) tools/packet_smoke.py $(QEMU) build/test-packet.iso 128M
build/packet-unit-test: tests/packet.c kernel/v7/packet.c kernel/include/packet.h include/nexix/packet.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/packet.c kernel/v7/packet.c -o $@
.PHONY: test-packet-unit
test-packet-unit: build/packet-unit-test build/packet-owner-test build/serial-unit-test build/packet-libc-test
	./build/packet-unit-test
	./build/packet-owner-test
	./build/serial-unit-test
	./build/packet-libc-test
test-unit: test-packet-unit
build/packet-owner-test: tests/packet-owner.c kernel/v7/packet.c kernel/v7/packet_tty.c kernel/include/packet.h include/nexix/packet.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/packet-owner.c kernel/v7/packet.c kernel/v7/packet_tty.c -o $@
build/serial-unit-test: tests/serial.c tests/uart/io.h kernel/serial.c kernel/include/serial.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Itests/uart -Ikernel/include -Iinclude tests/serial.c kernel/serial.c -o $@
build/packet-libc-unit.o: libc/packet.c libc/include/sys/pk.h libc/include/sys/ioctl.h include/nexix/packet.h | build/.dir
	$(CC) $(USER_CFLAGS) -Derrno=nx_packet_errno -Dioctl=nx_packet_ioctl -Dclose=nx_packet_close -Dwrite=nx_packet_write -c $< -o $@
build/packet-libc-test: tests/packet-libc.c build/packet-libc-unit.o | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -no-pie -Iinclude $^ -o $@

# Task 69: native memory/special devices.
build/memdev.o: kernel/v7/memdev.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/memdev.o
build/nexix.elf: build/memdev.o
-include build/memdev.d
build/memdev-unit-test: tests/memdev.c kernel/v7/memdev.c kernel/include/memdev.h include/nexix/memdev.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -no-pie -Ikernel/include -Iinclude tests/memdev.c kernel/v7/memdev.c -o $@
.PHONY: test-memdev-unit test-memdev
test-memdev-unit: build/memdev-unit-test
	./build/memdev-unit-test
test-unit: test-memdev-unit
test-memdev: test-memdev-unit test-rootfs test-inventory test-libc-interfaces
build/memdev_program.o: user/memdev-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/memdev_program.d

# Task 70: V7 disk families mapped to native media.
build/disk.o: kernel/v7/disk.c kernel/v7/disk_order.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/disk.o
build/nexix.elf: build/disk.o
-include build/disk.d
build/disk_native.o: kernel/disk_native.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/disk_native.o
build/nexix.elf: build/disk_native.o
-include build/disk_native.d
build/disk-unit-test: tests/disk.c kernel/v7/disk.c kernel/v7/disk_order.c kernel/v7/storage.c kernel/v7/dectape.c kernel/block.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $^ -o $@
.PHONY: test-disk-unit test-disk
test-disk-unit: build/disk-unit-test build/ide-test build/ide-bus-test
	./build/disk-unit-test
	./build/ide-test
	./build/ide-bus-test
test-unit: test-disk-unit
build/disk_program.o: user/disk-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/disk_program.d
build/ide-bus-test: tests/ide-bus.c kernel/ide.c kernel/block.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Itests/ide -Ikernel/include -Iinclude $^ -o $@
test-disk: test-disk-unit test-rootfs test-inventory test-libc-interfaces
	$(PYTHON) tools/disk_smoke.py $(QEMU) build/test-selftest.iso 16M
	$(PYTHON) tools/disk_smoke.py $(QEMU) build/test-selftest.iso 128M

# Task 71: sequential ht/tm records backed by regular files.
build/tape.o: kernel/v7/tape.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/tape_device.o: kernel/v7/tape_device.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/tape.o build/tape_device.o
build/nexix.elf: build/tape.o build/tape_device.o
build/tape_program.o: user/tape-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/tape.d build/tape_device.d build/tape_program.d
build/tape-unit-test: tests/tape.c kernel/v7/tape.c kernel/include/tape.h include/nexix/tape.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/tape.c kernel/v7/tape.c -o $@
.PHONY: test-tape-unit test-tape
test-tape-unit: build/tape-unit-test build/tape-device-test
	./build/tape-unit-test
	./build/tape-device-test
test-unit: test-tape-unit
test-tape: test-tape-unit test-rootfs test-inventory test-libc-interfaces
	$(PYTHON) tools/tape_smoke.py $(QEMU) build/test-selftest.iso 16M
	$(PYTHON) tools/tape_smoke.py $(QEMU) build/test-selftest.iso 128M
build/tape-device-test: tests/tape-device.c kernel/v7/tape.c kernel/v7/tape_device.c kernel/v7/dectape.c kernel/v7/dectape_media.c kernel/include/tape.h include/nexix/tape.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/tape-device.c kernel/v7/tape.c kernel/v7/tape_device.c kernel/v7/dectape.c kernel/v7/dectape_media.c -o $@

# Task 72: TC-11/TU56 random-block DECtape equivalent.
build/dectape.o: kernel/v7/dectape.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/dectape_media.o: kernel/v7/dectape_media.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/dectape.o build/dectape_media.o
build/nexix.elf: build/dectape.o build/dectape_media.o
build/dectape_program.o: user/dectape-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/dectape.d build/dectape_media.d build/dectape_program.d
build/dectape-unit-test: tests/dectape.c kernel/v7/dectape.c kernel/v7/storage.c kernel/v7/disk.c kernel/v7/disk_order.c kernel/block.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $^ -o $@
.PHONY: test-dectape-unit test-dectape
test-dectape-unit: build/dectape-unit-test
	./build/dectape-unit-test
test-unit: test-dectape-unit
test-dectape: test-dectape-unit test-tape-unit test-rootfs test-inventory test-libc-interfaces
	$(PYTHON) tools/dectape_smoke.py $(QEMU) build/test-selftest.iso 16M
	$(PYTHON) tools/dectape_smoke.py $(QEMU) build/test-selftest.iso 128M

# Task 73: V7 request ordering and HP drive interleave.
build/disk_order.o: kernel/v7/disk_order.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/disk_order.o
build/nexix.elf: build/disk_order.o
build/disk_order_program.o: user/disk-order-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/disk_order.d build/disk_order_program.d
build/disk-order-unit-test: tests/disk-order.c kernel/v7/disk_order.c kernel/v7/disk.c kernel/v7/storage.c kernel/v7/dectape.c kernel/block.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $^ -o $@
.PHONY: test-disk-order-unit test-disk-order
test-disk-order-unit: build/disk-order-unit-test
	./build/disk-order-unit-test
	$(PYTHON) tools/check_disk_order.py --cc "$(HOST_CC)"
test-unit: test-disk-order-unit
test-disk-order: test-disk-order-unit test-rootfs test-inventory test-libc-interfaces
	$(PYTHON) tools/disk_order_smoke.py $(QEMU) build/test-selftest.iso 16M
	$(PYTHON) tools/disk_order_smoke.py $(QEMU) build/test-selftest.iso 128M

# Task 74: native historical terminal-controller contracts.
build/tty_controller.o: kernel/v7/tty_controller.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/tty_controller.o
build/nexix.elf: build/tty_controller.o
-include build/tty_controller.d

build/tty-controller-test: tests/tty-controller.c tests/packet-inactive.c kernel/v7/tty.c kernel/v7/tty_controller.c kernel/include/tty_controller.h kernel/include/tty.h include/nexix/tty.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $(filter %.c,$^) -o $@
.PHONY: test-tty-controller-unit test-tty-controllers
test-tty-controller-unit: build/tty-controller-test build/serial-unit-test
	$(PYTHON) tools/check_tty_controller.py --cc "$(HOST_CC)"
	./build/tty-controller-test
	./build/serial-unit-test
test-unit: test-tty-controller-unit
test-tty-controllers: test-tty-controller-unit test-rootfs test-inventory test-libc-interfaces test-tty test-packet

build/tty_controller_program.o: user/tty-controller-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/tty_controller_program.d

# Task 75: DN/DU endpoints and DH modem/carrier support.
build/modem.o: kernel/v7/modem.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/modem.o
build/nexix.elf: build/modem.o
-include build/modem.d
build/modem-unit-test: tests/modem.c tests/packet-inactive.c kernel/v7/tty.c kernel/v7/tty_controller.c kernel/v7/modem.c kernel/include/modem.h include/nexix/modem.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $(filter %.c,$^) -o $@
.PHONY: test-modem-unit test-modem
test-modem-unit: build/modem-unit-test build/serial-unit-test
	./build/modem-unit-test
	./build/serial-unit-test
test-unit: test-modem-unit
test-modem: test-modem-unit test-rootfs test-inventory test-libc-interfaces test-tty test-packet

build/modem_program.o: user/modem-test.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/modem_program.d

# Task 83: manifest-driven installation staging and root-image equivalence.
.PHONY: test-install-unit test-install
test-install-unit: v7-userland
	$(PYTHON) tests/install_manifest.py
test-install: test-install-unit stage test-rootfs test-inventory test-libc-interfaces
test-unit: test-install-unit

# Task 84: release-scale V7 image sizing and native filesystem acceptance.
build/image-scale-test: tests/image-scale.c kernel/v7/fs.c kernel/block.c kernel/include/v7/fs.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $(filter %.c,$^) -o $@
build/image-scale.img: tools/mk_large_image_fixture.py tools/mk_rootfs.py tools/mk_v7fs.py tools/install_manifest.py docs/native-install.json $(INSTALL_INPUTS) | build/.dir
	$(PYTHON) tools/mk_large_image_fixture.py $@
.PHONY: test-image-builder-unit test-image-builder
test-image-builder-unit: v7-userland build/image-scale-test
	$(PYTHON) tests/image_builder.py
test-unit: test-image-builder-unit
test-image-builder: test-image-builder-unit test-rootfs test-inventory test-libc-interfaces multi-iso
	$(PYTHON) tools/image_scale_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/image_scale_smoke.py $(QEMU) build/nexix-multi.iso 128M

# Task 85: independent full IDE seed and fixed embedded bootstrap.
.PHONY: test-root-separation-unit test-root-separation
test-root-separation-unit: build/nexix.elf
	$(PYTHON) tests/root_separation.py
test-unit: test-root-separation-unit
test-root-separation: test-root-separation-unit rootfs test-rootfs test-inventory test-libc-interfaces multi-iso
	$(PYTHON) tools/root_separation_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/root_separation_smoke.py $(QEMU) build/nexix-multi.iso 128M

# Task 86: manifest-driven disposable guest acceptance.
.PHONY: test-guest-harness test-guest-harness-unit
test-guest-harness-unit:
	$(PYTHON) tests/guest_harness.py
test-unit: test-guest-harness-unit
test-guest-harness: rootfs multi-iso test-guest-harness-unit
	$(PYTHON) tools/guest_acceptance.py tests/guest/foundation.json --qemu $(QEMU) --iso build/nexix-multi.iso --ram 16M --report build/guest-16M.json
	$(PYTHON) tools/guest_acceptance.py tests/guest/foundation.json --qemu $(QEMU) --iso build/nexix-multi.iso --ram 128M --report build/guest-128M.json

# Task 89: per-line TTY state and transport ownership.
build/tty-lines-test: tests/tty-lines.c tests/tty-controller.c tests/packet-inactive.c kernel/v7/tty.c kernel/v7/tty_controller.c kernel/include/tty.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/tty-lines.c tests/packet-inactive.c kernel/v7/tty.c kernel/v7/tty_controller.c -o $@
.PHONY: test-tty-lines-unit
test-tty-lines-unit: build/tty-lines-test
	./build/tty-lines-test
test-unit: test-tty-lines-unit

# Task 90: second PC UART and IRQ3 routing.
build/serial-secondary-test: tests/serial-secondary.c tests/uart/io.h kernel/serial_secondary.c kernel/include/serial.h kernel/include/tty.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Itests/uart -Ikernel/include -Iinclude tests/serial-secondary.c kernel/serial_secondary.c -o $@
build/serial-secondary-guest.o: tests/serial-secondary-guest.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/serial-secondary-guest.elf: build/crt0.o build/serial-secondary-guest.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-serial-secondary-unit test-serial-secondary
test-serial-secondary-unit: build/serial-secondary-test
	./build/serial-secondary-test
test-unit: test-serial-secondary-unit
test-serial-secondary: test-serial-secondary-unit rootfs multi-iso build/serial-secondary-guest.elf
	$(PYTHON) tools/serial_secondary_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/serial_secondary_smoke.py $(QEMU) build/nexix-multi.iso 128M

# Task 91: controlling-terminal and signal/flow isolation on both real UARTs.
build/terminal-sessions.o: tests/terminal-sessions.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/terminal-sessions.elf: build/crt0.o build/terminal-sessions.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-terminal-sessions
test-terminal-sessions: rootfs multi-iso build/terminal-sessions.elf
	$(PYTHON) tools/terminal_sessions_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/terminal_sessions_smoke.py $(QEMU) build/nexix-multi.iso 128M

# Task 92: installed COM2 device and the default host endpoint.
.PHONY: test-terminal-config-unit test-terminal-config
test-terminal-config-unit:
	$(PYTHON) tests/serial_terminal.py
test-unit: test-terminal-config-unit
test-terminal-config: test-terminal-config-unit rootfs multi-iso build/serial-secondary-guest.elf
	$(PYTHON) tools/serial_secondary_smoke.py $(QEMU) build/nexix-multi.iso 16M --packaged
	$(PYTHON) tools/serial_secondary_smoke.py $(QEMU) build/nexix-multi.iso 128M --packaged

# Task 93: real concurrent shells and repeated resource recovery.
build/terminal-shells.o: tests/terminal-shells.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/terminal-shells.elf: build/crt0.o build/terminal-shells.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-two-terminals
test-two-terminals: test-serial-secondary test-terminal-sessions test-terminal-config build/terminal-shells.elf
	$(PYTHON) tools/terminal_shells_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/terminal_shells_smoke.py $(QEMU) build/nexix-multi.iso 128M

# Task 94: native account and startup configuration.
build/account-config.o: tests/account-config.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/account-config.elf: build/crt0.o build/account-config.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-account-config
test-account-config: rootfs multi-iso build/account-config.elf test-install-unit test-root-separation-unit
	$(PYTHON) tools/account_config_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/account_config_smoke.py $(QEMU) build/nexix-multi.iso 128M

# Task 95: retained stty command against independent native UARTs.
build/stty-command.o: tests/stty-command.c tests/command-runner.h | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/stty-command.elf: build/crt0.o build/stty-command.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-stty
test-stty: rootfs multi-iso build/stty-command.elf
	$(PYTHON) tools/stty_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/stty_smoke.py $(QEMU) build/nexix-multi.iso 128M

.PHONY: test-tty-command
test-tty-command: rootfs multi-iso
	$(PYTHON) tools/guest_acceptance.py tests/guest/tty.json --qemu $(QEMU) --iso build/nexix-multi.iso --ram 16M --report build/tty-command-16M.json
	$(PYTHON) tools/guest_acceptance.py tests/guest/tty.json --qemu $(QEMU) --iso build/nexix-multi.iso --ram 128M --report build/tty-command-128M.json

build/mesg-command.o: tests/mesg-command.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/mesg-command.elf: build/crt0.o build/mesg-command.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-mesg
test-mesg: rootfs multi-iso build/mesg-command.elf
	$(PYTHON) tools/mesg_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/mesg_smoke.py $(QEMU) build/nexix-multi.iso 128M

build/getty-handoff.o: tests/getty-handoff.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/getty-handoff.elf: build/crt0.o build/getty-handoff.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-getty
test-getty: rootfs multi-iso build/getty-handoff.elf
	$(PYTHON) tools/getty_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/getty_smoke.py $(QEMU) build/nexix-multi.iso 128M

build/login-identity.o: tests/login-identity.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/login-identity.elf: build/crt0.o build/login-identity.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-login
test-login: rootfs multi-iso build/login-identity.elf test-libc-interfaces
	$(PYTHON) tools/login_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/login_smoke.py $(QEMU) build/nexix-multi.iso 128M

build/session-records.o: tests/session-records.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/session-records.elf: build/crt0.o build/session-records.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-session-accounting
test-session-accounting: rootfs multi-iso build/session-records.elf
	$(PYTHON) tools/session_accounting_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/session_accounting_smoke.py $(QEMU) build/nexix-multi.iso 128M

build/init-control.o: tests/init-control.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/init-control.elf: build/crt0.o build/init-control.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-multiuser-init
test-multiuser-init: rootfs multi-iso build/init-control.elf
	$(PYTHON) tools/multiuser_init_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/multiuser_init_smoke.py $(QEMU) build/nexix-multi.iso 128M

build/v7-password-file.o: user/v7/password-file.c user/v7/password-file.h | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
-include build/v7-password-file.d
build/v7-passwd.elf: build/v7-password-file.o
build/password-file-unit.o: user/v7/password-file.c user/v7/password-file.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -D_DEFAULT_SOURCE -Dchown=nx_password_chown -Dsync=nx_password_sync -Dlink=nx_password_link -Dunlink=nx_password_unlink -c $< -o $@
build/password-file-test: tests/password-file.c build/password-file-unit.o
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $^ -o $@
build/password-state.o: tests/password-state.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/password-state.elf: build/crt0.o build/password-state.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-passwd-command-unit test-passwd-command
test-passwd-command-unit: build/password-file-test
	./build/password-file-test
test-unit: test-passwd-command-unit
test-passwd-command: test-passwd-command-unit rootfs multi-iso build/password-state.elf
	$(PYTHON) tools/passwd_command_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/passwd_command_smoke.py $(QEMU) build/nexix-multi.iso 128M

build/credentials.o: tests/credentials.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/credentials.elf: build/crt0.o build/credentials.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-su
test-su: rootfs multi-iso build/credentials.elf
	$(PYTHON) tools/su_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/su_smoke.py $(QEMU) build/nexix-multi.iso 128M

.PHONY: test-newgrp
test-newgrp: rootfs multi-iso build/credentials.elf
	$(PYTHON) tools/newgrp_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/newgrp_smoke.py $(QEMU) build/nexix-multi.iso 128M

.PHONY: test-who
test-who: rootfs multi-iso
	$(PYTHON) tools/who_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/who_smoke.py $(QEMU) build/nexix-multi.iso 128M

.PHONY: test-write
test-write: rootfs multi-iso
	$(PYTHON) tools/write_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/write_smoke.py $(QEMU) build/nexix-multi.iso 128M

.PHONY: test-wall
test-wall: rootfs multi-iso
	$(PYTHON) tools/wall_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/wall_smoke.py $(QEMU) build/nexix-multi.iso 128M

build/session-resources.o: tests/session-resources.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/session-resources.elf: build/crt0.o build/session-resources.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-multiuser-acceptance test-multiuser
test-multiuser-acceptance: rootfs multi-iso build/session-resources.elf
	$(PYTHON) tools/multiuser_acceptance.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/multiuser_acceptance.py $(QEMU) build/nexix-multi.iso 128M
test-multiuser: test-two-terminals test-account-config test-stty test-tty-command test-mesg test-getty test-login test-session-accounting test-multiuser-init test-passwd-command test-su test-newgrp test-who test-write test-wall test-multiuser-acceptance

.PHONY: test-v7-archives extract-v7-archives
test-v7-archives:
	$(PYTHON) tests/test_v7_archive.py

test-unit: test-v7-archives

# Explicit extraction refuses to overwrite an existing directory.
extract-v7-archives:
	$(PYTHON) tools/v7_archive.py --output $(ARCHIVE_OUTPUT)
ARCHIVE_OUTPUT ?= build/v7-archives

build/fpsim-equivalent.o: tests/fpsim-equivalent.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/fpsim-equivalent.elf: build/crt0.o build/fpsim-equivalent.o build/libm.a build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o,$^) $(filter %.a,$^) -o $@
.PHONY: test-fpsim-equivalent
test-fpsim-equivalent: build/nexix-multi.iso build/fpsim-equivalent.elf stage
	$(PYTHON) tools/fpsim_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/fpsim_smoke.py $(QEMU) build/nexix-multi.iso 128M

build/runtime-equivalent.o: tests/runtime-equivalent.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/runtime-equivalent.elf: build/crt0.o build/runtime-equivalent.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o,$^) $(filter %.a,$^) -o $@
.PHONY: test-runtime-equivalent
test-runtime-equivalent: build/nexix-multi.iso build/runtime-equivalent.elf stage
	$(PYTHON) tools/runtime_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/runtime_smoke.py $(QEMU) build/nexix-multi.iso 128M

.PHONY: test-mkconf
test-mkconf:
	$(PYTHON) tests/test_mkconf.py
test-unit: test-mkconf

build/output.o: kernel/v7/output.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/output.o
build/nexix.elf: build/output.o
-include build/output.d

build/typesetter.o: tests/typesetter.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/typesetter.elf: build/crt0.o build/typesetter.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o,$^) $(filter %.a,$^) -o $@
.PHONY: test-typesetter
test-typesetter: build/nexix-multi.iso build/typesetter.elf stage
	$(PYTHON) tools/typesetter_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/typesetter_smoke.py $(QEMU) build/nexix-multi.iso 128M

build/output-unit-test: tests/output-unit.c kernel/v7/output.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude $^ -o $@
.PHONY: test-output-unit
test-output-unit: build/output-unit-test
	./build/output-unit-test
test-unit: test-output-unit
test-typesetter: test-output-unit

build/printer.o: tests/printer.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/printer.elf: build/crt0.o build/printer.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o,$^) $(filter %.a,$^) -o $@
.PHONY: test-printer
test-printer: build/nexix-multi.iso build/printer.elf stage test-output-unit
	$(PYTHON) tools/printer_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/printer_smoke.py $(QEMU) build/nexix-multi.iso 128M

build/math-coverage.o: tests/math-coverage.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/math-coverage.elf: build/crt0.o build/math-coverage.o build/libm.a build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o,$^) $(filter %.a,$^) -o $@
.PHONY: test-math-coverage
test-math-coverage: build/nexix-multi.iso build/math-coverage.elf stage
	$(PYTHON) tools/math_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/math_smoke.py $(QEMU) build/nexix-multi.iso 128M

.PHONY: test-supplemental-inputs
test-supplemental-inputs:
	$(PYTHON) tests/supplemental_inputs.py
test-unit: test-supplemental-inputs

build/exec-search-unit.o: libc/exec.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Dexecv=nx_test_execv -Dexecvp=nx_test_execvp -Dgetenv=nx_test_getenv -Dsleep=nx_test_sleep -Dexecl=nx_test_execl -Dexecle=nx_test_execle -Dexeclp=nx_test_execlp -c $< -o $@
build/exec-search-unit-test: tests/exec-search-unit.c build/exec-search-unit.o
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $^ -o $@
.PHONY: test-exec-search-unit
test-exec-search-unit: build/exec-search-unit-test
	./build/exec-search-unit-test
test-unit: test-exec-search-unit

# Native development interfaces are staged only in the full IDE distribution.
build/fcrt0.o: libc/arch/x86_64/fcrt0.S libc/arch/x86_64/crt0.S | build/.dir
	$(CC) -m64 -ffreestanding -fno-pie -c $< -o $@
build/dev-include/nexix/%: include/nexix/% | build/.dir
	mkdir -p $(@D)
	cp $< $@
build/dev-include/%: libc/include/% | build/.dir
	mkdir -p $(@D)
	cp $< $@

# Compile/link from the verified installed tree, with no compiler/host headers.
DEVKIT_CFLAGS = $(filter-out -Ilibc/include -Iinclude -isystem $(shell $(CC) -print-file-name=include),$(USER_CFLAGS)) -I$(INSTALL_STAGE)/root/usr/include
build/devkit-consumer.o: tests/development-consumer.c $(INSTALL_STAGE)/receipt.json
	$(CC) $(DEVKIT_CFLAGS) -pg -fno-optimize-sibling-calls -c $< -o $@
build/devkit-%.elf: build/devkit-consumer.o $(INSTALL_STAGE)/receipt.json user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(INSTALL_STAGE)/root/lib/$*.o $< $(INSTALL_STAGE)/root/lib/libm.a $(INSTALL_STAGE)/root/lib/libc.a -o $@
build/devkit-cat.o: user/v7/cmd/cat.c user/v7/compat.h $(INSTALL_STAGE)/receipt.json
	$(CC) $(filter-out -std=c11 -Werror,$(DEVKIT_CFLAGS)) -std=gnu89 -Wno-implicit-int -Wno-implicit-function-declaration -Wno-return-type -include user/v7/compat.h -c $< -o $@
build/devkit-cat.elf: build/devkit-cat.o $(INSTALL_STAGE)/receipt.json user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(INSTALL_STAGE)/root/lib/crt0.o $< $(INSTALL_STAGE)/root/lib/libc.a -o $@
build/exec-consumer.o: tests/exec-consumer.c $(INSTALL_STAGE)/receipt.json
	$(CC) $(DEVKIT_CFLAGS) -c $< -o $@
build/exec-consumer.elf: build/exec-consumer.o $(INSTALL_STAGE)/receipt.json user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(INSTALL_STAGE)/root/lib/crt0.o $< $(INSTALL_STAGE)/root/lib/libc.a -o $@
.PHONY: test-development
test-development: build/nexix-multi.iso build/devkit-crt0.elf build/devkit-fcrt0.elf build/devkit-mcrt0.elf build/devkit-fmcrt0.elf build/devkit-cat.elf build/exec-consumer.elf stage test-exec-search-unit
	$(PYTHON) tools/development_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/development_smoke.py $(QEMU) build/nexix-multi.iso 128M

.PHONY: test-libc-source-coverage
test-libc-source-coverage:
	$(PYTHON) tests/libc_source_coverage.py
test-inventory: test-libc-source-coverage

.PHONY: test-support-inputs
test-support-inputs: test-supplemental-inputs
	$(PYTHON) tests/support_inputs.py --cc $(HOST_CC)
test-unit: test-support-inputs

build/vga.o: kernel/vga.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/vga-test: tests/vga.c kernel/vga.c kernel/include/vga.h tests/vga/io.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Itests/vga -Ikernel/include tests/vga.c kernel/vga.c -o $@
.PHONY: test-vga-unit
test-vga-unit: build/vga-test
	./build/vga-test
test-unit: test-vga-unit

build/keyboard.o: kernel/keyboard.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
build/keyboard-test: tests/keyboard.c kernel/keyboard.c kernel/include/keyboard.h tests/keyboard/io.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Itests/keyboard -Ikernel/include tests/keyboard.c kernel/keyboard.c -o $@
.PHONY: test-keyboard-unit
test-keyboard-unit: build/keyboard-test
	./build/keyboard-test
test-unit: test-keyboard-unit


build/local_console.o: kernel/local_console.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: run-vga
run: run-$(CONSOLE)
run-vga: build
	$(PYTHON) tools/serial_terminal.py --prepare build/ttyS1
	$(QEMU) -machine pc -accel tcg -cpu qemu64 -smp 1 -m 128M -rtc base=utc -boot d -drive "file=$(DISK_IMAGE),format=raw,if=ide,index=0" -cdrom build/nexix.iso -vga std -display gtk -chardev stdio,id=console,mux=on,signal=off -serial chardev:console -serial pipe:build/ttyS1 -mon chardev=console,mode=readline -no-reboot

.PHONY: test-vga
test-vga: test-vga-unit test-keyboard-unit test-tty-lines-unit multi-iso rootfs
	$(PYTHON) tools/vga_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/vga_smoke.py $(QEMU) build/nexix-multi.iso 128M

.PHONY: test-basename
test-basename: multi-iso rootfs
	$(PYTHON) tools/guest_acceptance.py tests/guest/basename.json --qemu $(QEMU) --iso build/nexix-multi.iso --ram 16M --report build/basename-16M.json
	$(PYTHON) tools/guest_acceptance.py tests/guest/basename.json --qemu $(QEMU) --iso build/nexix-multi.iso --ram 128M --report build/basename-128M.json

.PHONY: test-cal
test-cal: multi-iso rootfs
	$(PYTHON) tools/guest_acceptance.py tests/guest/cal.json --qemu $(QEMU) --iso build/nexix-multi.iso --ram 16M --report build/cal-16M.json
	$(PYTHON) tools/guest_acceptance.py tests/guest/cal.json --qemu $(QEMU) --iso build/nexix-multi.iso --ram 128M --report build/cal-128M.json

build/%-command.o: tests/%-command.c tests/command-fixture.h tests/command-runner.h | build/.dir
	$(CC) $(USER_CFLAGS) -Wno-unused-function -c $< -o $@
build/%-command.elf: build/crt0.o build/%-command.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@

.PHONY: test-chgrp
test-chgrp: multi-iso rootfs build/chgrp-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M chgrp
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M chgrp

.PHONY: test-chmod
test-chmod: multi-iso rootfs build/chmod-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M chmod
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M chmod

.PHONY: test-chown
test-chown: multi-iso rootfs build/chown-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M chown
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M chown

.PHONY: test-expr
test-expr: multi-iso rootfs build/expr-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M expr
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M expr

.PHONY: test-file
test-file: multi-iso rootfs build/file-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M file
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M file

.PHONY: test-kill
test-kill: multi-iso rootfs build/kill-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M kill
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M kill

.PHONY: test-mkdir
test-mkdir: multi-iso rootfs build/mkdir-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M mkdir
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M mkdir

.PHONY: test-mv
test-mv: multi-iso rootfs build/mv-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M mv
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M mv

.PHONY: test-nice
test-nice: multi-iso rootfs build/nice-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M nice
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M nice

.PHONY: test-osh
test-osh: multi-iso rootfs build/osh-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M osh
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M osh

.PHONY: test-rmdir
test-rmdir: multi-iso rootfs build/rmdir-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M rmdir
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M rmdir

.PHONY: test-sleep
test-sleep: multi-iso rootfs build/sleep-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M sleep
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M sleep

.PHONY: test-sync
test-sync: multi-iso rootfs
	$(PYTHON) tools/sync_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/sync_smoke.py $(QEMU) build/nexix-multi.iso 128M

.PHONY: test-test
test-test: multi-iso rootfs build/test-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M test
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M test

.PHONY: test-yes
test-yes: multi-iso rootfs build/yes-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M yes
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M yes

.PHONY: test-false
test-false: multi-iso rootfs
	$(PYTHON) tools/guest_acceptance.py tests/guest/false.json --qemu $(QEMU) --iso build/nexix-multi.iso --ram 16M --report build/false-16M.json
	$(PYTHON) tools/guest_acceptance.py tests/guest/false.json --qemu $(QEMU) --iso build/nexix-multi.iso --ram 128M --report build/false-128M.json

.PHONY: test-true
test-true: multi-iso rootfs
	$(PYTHON) tools/guest_acceptance.py tests/guest/true.json --qemu $(QEMU) --iso build/nexix-multi.iso --ram 16M --report build/true-16M.json
	$(PYTHON) tools/guest_acceptance.py tests/guest/true.json --qemu $(QEMU) --iso build/nexix-multi.iso --ram 128M --report build/true-128M.json

.PHONY: test-nohup
test-nohup: multi-iso rootfs build/nohup-command.elf
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 16M nohup
	$(PYTHON) tools/command_smoke.py $(QEMU) build/nexix-multi.iso 128M nohup

.PHONY: test-essential
test-essential: test-basename test-cal test-chgrp test-chmod test-chown test-expr test-file test-kill test-mkdir test-mv test-nice test-osh test-rmdir test-sleep test-sync test-test test-yes test-false test-true test-nohup

.PHONY: test-expr-bounds
test-expr-bounds:
	$(PYTHON) tests/expr_bounds.py --cc "$(HOST_CC)"
test-expr: test-expr-bounds

# Separate filenames prevent stale console settings when switching modes.
build/nexix-serial.iso build/nexix-multi.iso: build/nexix.elf boot/grub.cfg tools/pack_kernel.py
	mkdir -p $(@:.iso=)/boot/grub
	$(PYTHON) tools/pack_kernel.py build/nexix.elf $(@:.iso=)/boot/nexix.elf
	sed 's|set nexix_console=vga|set nexix_console=$(if $(findstring serial,$@),serial,multi)|' boot/grub.cfg > $(@:.iso=)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(@:.iso=)
.PHONY: run-serial serial-iso multi-iso

build/console-test: tests/console.c kernel/console.c kernel/include/console.h kernel/include/boot.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/console.c kernel/console.c -o $@
.PHONY: test-console-unit test-console
test-console-unit: build/console-test build/memory-test test-tty-lines-unit
	./build/console-test
	./build/memory-test
test-unit: test-console-unit
test-console: test-console-unit iso serial-iso rootfs
	$(PYTHON) tools/console_smoke.py $(QEMU) 16M
	$(PYTHON) tools/console_smoke.py $(QEMU) 128M
test: test-console

build/klog-test: tests/klog.c kernel/klog.c kernel/include/klog.h include/nexix/klog.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/klog.c kernel/klog.c -o $@
.PHONY: test-klog-unit test-dmesg
test-klog-unit: build/klog-test build/console-test
	./build/klog-test
	./build/console-test
test-unit: test-klog-unit
test-dmesg: test-klog-unit multi-iso rootfs
	$(PYTHON) tools/dmesg_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/dmesg_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-dmesg

# Task 357: explicit offline upgrades never overwrite an existing backup.
UPGRADE_BACKUP ?= $(DISK_IMAGE).bak
.PHONY: install-disk upgrade-preview upgrade-disk test-upgrade-unit test-upgrade
install-disk: disk
upgrade-preview: stage
	$(PYTHON) tools/upgrade_disk.py --image "$(DISK_IMAGE)" --stage "$(INSTALL_STAGE)" --manifest "$(INSTALL_MANIFEST)"
upgrade-disk: stage
	$(PYTHON) tools/upgrade_disk.py --image "$(DISK_IMAGE)" --stage "$(INSTALL_STAGE)" --manifest "$(INSTALL_MANIFEST)" --apply --backup "$(UPGRADE_BACKUP)"
test-upgrade-unit:
	$(PYTHON) tests/upgrade_disk.py
test-unit: test-upgrade-unit
test-upgrade: test-upgrade-unit serial-iso rootfs
	$(PYTHON) tools/upgrade_smoke.py $(QEMU) build/nexix-serial.iso 16M
	$(PYTHON) tools/upgrade_smoke.py $(QEMU) build/nexix-serial.iso 128M
test: test-upgrade

.PHONY: test-ed-unit test-ed
test-ed-unit:
	$(PYTHON) tests/ed.py --cc $(HOST_CC)
test-unit: test-ed-unit
test-ed: test-ed-unit multi-iso rootfs
	$(PYTHON) tools/ed_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/ed_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-ed
build/ed-recover.o: tests/ed-recover.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/ed-recover.elf: build/crt0.o build/ed-recover.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/ed-recover.o build/libc.a -o $@
-include build/ed-recover.d
test-ed: build/ed-recover.elf

.PHONY: test-grep-unit test-grep
test-grep-unit:
	python3 tests/grep.py
test-unit: test-grep-unit
test-grep: test-grep-unit multi-iso rootfs
	python3 tools/grep_smoke.py $(QEMU) build/nexix-multi.iso 16M
	python3 tools/grep_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-grep

.PHONY: test-tail-unit test-tail
test-tail-unit:
	python3 tests/tail.py
test-unit: test-tail-unit
test-tail: test-tail-unit multi-iso rootfs
	python3 tools/tail_smoke.py $(QEMU) build/nexix-multi.iso 16M
	python3 tools/tail_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-tail

.PHONY: test-cmp-unit test-cmp
test-cmp-unit:
	python3 tests/cmp.py
test-unit: test-cmp-unit
test-cmp: test-cmp-unit multi-iso rootfs
	python3 tools/cmp_smoke.py $(QEMU) build/nexix-multi.iso 16M
	python3 tools/cmp_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-cmp

.PHONY: test-od-unit test-od
test-od-unit:
	python3 tests/od.py
test-unit: test-od-unit
test-od: test-od-unit multi-iso rootfs
	python3 tools/od_smoke.py $(QEMU) build/nexix-multi.iso 16M
	python3 tools/od_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-od

.PHONY: test-wc-unit test-wc
test-wc-unit:
	python3 tests/wc.py
test-unit: test-wc-unit
test-wc: test-wc-unit multi-iso rootfs
	python3 tools/wc_smoke.py $(QEMU) build/nexix-multi.iso 16M
	python3 tools/wc_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-wc

# Task 168: retained V7 dependency engine plus native parser/archive adapters.
V7_MAKE_SOURCES = main doname misc files dosys ident parser archive
V7_MAKE_OBJECTS = $(addprefix build/v7-make-,$(addsuffix .o,$(V7_MAKE_SOURCES)))
-include $(V7_MAKE_OBJECTS:.o=.d)
build/v7-make-%.o: user/v7/cmd/make/%.c user/v7/cmd/make/defs | build/.dir
	$(CC) $(V7_CFLAGS) -Dunix -c $< -o $@
build/v7-make.elf: $(V7_MAKE_OBJECTS) build/crt0.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o $(V7_MAKE_OBJECTS) build/libc.a -o $@

build/make-time.o: tests/make-time.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/make-time.elf: build/make-time.o build/crt0.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/make-time.o build/libc.a -o $@
build/make-member.o: tests/make-member.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/make-lib.a: build/make-member.o
	$(AR) rcU $@ $<
.PHONY: test-make-unit test-make
test-make-unit:
	python3 tests/make.py
test-unit: test-make-unit
test-make: test-make-unit multi-iso rootfs build/make-time.elf build/make-lib.a
	python3 tools/make_smoke.py $(QEMU) build/nexix-multi.iso 16M
	python3 tools/make_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-make

.PHONY: test-ls
test-ls: multi-iso rootfs
	python3 tools/ls_smoke.py $(QEMU) build/nexix-multi.iso 16M
	python3 tools/ls_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-ls

# Native uname extension and actual kernel/libc ABI acceptance.
build/v7-uname.o: user/v7/cmd/uname.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/uname-api.o: tests/uname-api.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/uname-api.elf: build/uname-api.o build/crt0.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/uname-api.o build/libc.a -o $@
-include build/uname-api.d
.PHONY: test-uname-unit test-uname
test-uname-unit:
	python3 tests/uname.py
test-unit: test-uname-unit
test-uname: test-uname-unit multi-iso rootfs build/uname-api.elf
	python3 tools/uname_smoke.py $(QEMU) build/nexix-multi.iso 16M
	python3 tools/uname_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-uname

build/utsname.o: kernel/v7/utsname.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/utsname.o
build/nexix.elf: build/utsname.o
-include build/utsname.d
build/v7-hostname.o: user/v7/cmd/hostname.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/v7-sh-prompt.o: user/v7/sh/prompt.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/hostname-unit-test: tests/hostname.c kernel/v7/utsname.c include/nexix/hostname.h kernel/include/utsname.h kernel/include/version.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/hostname.c kernel/v7/utsname.c -o $@
.PHONY: test-hostname-unit test-hostname
test-hostname-unit: build/hostname-unit-test
	./build/hostname-unit-test
test-unit: test-hostname-unit

build/hostname-api.o: tests/hostname-api.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/hostname-api.elf: build/hostname-api.o build/crt0.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/hostname-api.o build/libc.a -o $@
-include build/hostname-api.d
test-hostname: test-hostname-unit multi-iso rootfs build/hostname-api.elf
	python3 tools/hostname_smoke.py $(QEMU) build/nexix-multi.iso 16M
	python3 tools/hostname_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-hostname

# Intermittent-freeze investigation on disposable IDE roots.
STABILITY_ROUNDS ?= 100
.PHONY: test-stability
test-stability: multi-iso rootfs
	$(PYTHON) tools/stability_smoke.py $(QEMU) build/nexix-multi.iso 16M --rounds $(STABILITY_ROUNDS)
	$(PYTHON) tools/stability_smoke.py $(QEMU) build/nexix-multi.iso 128M --rounds $(STABILITY_ROUNDS)

# Task 362: V7 resident scheduler policy.
build/scheduler.o: kernel/v7/scheduler.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/scheduler.o
build/nexix.elf: build/scheduler.o
-include build/scheduler.d
build/scheduler-unit-test: tests/scheduler.c kernel/v7/scheduler.c kernel/include/scheduler.h kernel/include/v7/core.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/scheduler.c kernel/v7/scheduler.c -o $@
.PHONY: test-scheduler-unit
test-scheduler-unit: build/scheduler-unit-test
	./build/scheduler-unit-test
test-unit: test-scheduler-unit

# Task 363: atomic native ftime.
build/ftime-api.o: tests/ftime-api.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/ftime-api.elf: build/ftime-api.o build/crt0.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/ftime-api.o build/libc.a -o $@
-include build/ftime-api.d
.PHONY: test-ftime
test-ftime: build/rtc-test build/time-test multi-iso rootfs build/ftime-api.elf
	./build/rtc-test
	./build/time-test
	$(PYTHON) tools/timing_api_smoke.py $(QEMU) build/nexix-multi.iso 16M ftime-api FTIME_API_OK
	$(PYTHON) tools/timing_api_smoke.py $(QEMU) build/nexix-multi.iso 128M ftime-api FTIME_API_OK
test: test-ftime

# Task 364: bounded native kernel timeout callbacks.
build/callout.o: kernel/v7/callout.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/callout.o
build/nexix.elf: build/callout.o
-include build/callout.d
build/callout-unit-test: tests/callout.c kernel/v7/callout.c kernel/include/callout.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/callout.c kernel/v7/callout.c -o $@
.PHONY: test-callout-unit
test-callout-unit: build/callout-unit-test
	./build/callout-unit-test
test-unit: test-callout-unit

# Task 365: sleep/alarm compatibility and atomic deadline waiting.
build/sleep-api.o: tests/sleep-api.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/sleep-api.elf: build/sleep-api.o build/crt0.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/sleep-api.o build/libc.a -o $@
-include build/sleep-api.d
.PHONY: test-sleep-api
test-sleep-api: multi-iso rootfs build/sleep-api.elf
	$(PYTHON) tools/timing_api_smoke.py $(QEMU) build/nexix-multi.iso 16M sleep-api SLEEP_API_OK
	$(PYTHON) tools/timing_api_smoke.py $(QEMU) build/nexix-multi.iso 128M sleep-api SLEEP_API_OK
test: test-sleep
build/sleep-unit-test: tests/sleep.c libc/sleep.c | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin -Derrno=nx_sleep_errno -Dsleep=nx_test_sleep -Dsignal=nx_test_signal -Dalarm=nx_test_alarm -Dtimes=nx_test_times -Ilibc/include -Iinclude tests/sleep.c libc/sleep.c -o $@
.PHONY: test-sleep-unit
test-sleep-unit: build/sleep-unit-test
	./build/sleep-unit-test
test-sleep: test-sleep-api test-sleep-unit
test-unit: test-sleep-unit

# Task 366: historical invalid/ignored syscall dispositions.
build/syscall_policy.o: kernel/v7/syscall_policy.c | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/syscall_policy.o
build/nexix.elf: build/syscall_policy.o
-include build/syscall_policy.d
build/syscall-policy-unit-test: tests/syscall-policy.c kernel/v7/syscall_policy.c kernel/include/syscall.h include/nexix/syscall.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/syscall-policy.c kernel/v7/syscall_policy.c -o $@
build/syscall-policy-api.o: tests/syscall-policy-api.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/syscall-policy-api.elf: build/syscall-policy-api.o build/crt0.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/syscall-policy-api.o build/libc.a -o $@
-include build/syscall-policy-api.d
.PHONY: test-syscall-policy-unit test-syscall-policy
test-syscall-policy-unit: build/syscall-policy-unit-test
	./build/syscall-policy-unit-test
test-syscall-policy: test-syscall-policy-unit multi-iso rootfs build/syscall-policy-api.elf
	$(PYTHON) tools/timing_api_smoke.py $(QEMU) build/nexix-multi.iso 16M syscall-policy-api SYSCALL_POLICY_API_OK
	$(PYTHON) tools/timing_api_smoke.py $(QEMU) build/nexix-multi.iso 128M syscall-policy-api SYSCALL_POLICY_API_OK
test-unit: test-syscall-policy-unit
test: test-syscall-policy

.PHONY: test-vi-reference
test-vi-reference:
	$(PYTHON) tests/vi_reference.py
test-inventory: test-vi-reference

LIBC_OBJECTS += build/libc_termcap.o
build/libc.a: build/libc_termcap.o
build/termcap-unit-test: tests/termcap.c libc/termcap.c libc/include/termcap.h | build/.dir
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined tests/termcap.c libc/termcap.c -o $@
.PHONY: test-termcap
test-termcap: build/termcap-unit-test
	ASAN_OPTIONS=detect_leaks=0 ./build/termcap-unit-test
test-unit: test-termcap

build/termcap-guest.o: tests/termcap-guest.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/termcap-guest.elf: build/crt0.o build/termcap-guest.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o,$^) $(filter %.a,$^) -o $@
.PHONY: test-termcap-guest
test-termcap-guest: build/nexix-serial.iso build/termcap-guest.elf stage
	$(PYTHON) tools/termcap_smoke.py $(QEMU) build/nexix-serial.iso 16M
	$(PYTHON) tools/termcap_smoke.py $(QEMU) build/nexix-serial.iso 128M

LIBC_OBJECTS += build/libc_termcap_output.o
build/libc.a: build/libc_termcap_output.o
build/termcap-output-unit-test: tests/termcap-output.c libc/termcap_output.c libc/include/termcap.h | build/.dir
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined tests/termcap-output.c libc/termcap_output.c -o $@
.PHONY: test-termcap-output
test-termcap-output: build/termcap-output-unit-test
	ASAN_OPTIONS=detect_leaks=0 ./build/termcap-output-unit-test
test-unit: test-termcap-output

build/vga-control.o: tests/vga-control.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/vga-control.elf: build/crt0.o build/vga-control.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o,$^) $(filter %.a,$^) -o $@
.PHONY: test-vga-controls
test-vga-controls: test-vga-unit build/nexix-multi.iso build/vga-control.elf stage
	$(PYTHON) tools/vga_control_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/vga_control_smoke.py $(QEMU) build/nexix-multi.iso 128M

build/v7-clear.o: user/v7/cmd/clear.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
.PHONY: test-clear
test-clear: build/nexix-multi.iso build/v7-clear.elf stage
	$(PYTHON) tools/clear_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/clear_smoke.py $(QEMU) build/nexix-multi.iso 128M

LIBC_OBJECTS += build/libc_termios.o
build/libc.a: build/libc_termios.o
build/termios-guest.o: tests/termios-guest.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/termios-guest.elf: build/crt0.o build/termios-guest.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-termios
test-termios: build/tty-unit-test build/nexix-multi.iso build/termios-guest.elf stage
	./build/tty-unit-test
	$(PYTHON) tools/termios_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/termios_smoke.py $(QEMU) build/nexix-multi.iso 128M

# Traditional Vi native working sources; preserved release inputs stay immutable.
VI_NAMES = ex ex_addr ex_cmds ex_cmds2 ex_cmdsub ex_data ex_extern ex_get ex_io ex_put ex_re ex_set ex_subr ex_tagio ex_temp ex_tty ex_unix ex_v ex_vadj ex_vget ex_vmain ex_voper ex_vops ex_vops2 ex_vops3 ex_vput ex_vwind printf ex_version malloc native
VI_OBJECTS = $(addprefix build/vi-,$(addsuffix .o,$(VI_NAMES)))
VI_CFLAGS = $(V7_CFLAGS) -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=int-conversion -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast -Iuser/vi -DLISPCODE -DCHDIR -DFASTTAG -DUCVISUAL -DBIT8 -DISO8859_1 -DVMUNIX -DLONG_BIT=64 -DEXRECOVER='"/usr/libexec/exrecover"' -DEXPRESERVE='"/usr/libexec/expreserve"' -DPRESERVEDIR='"/var/preserve"'
build/vi-%.o: user/vi/%.c $(wildcard user/vi/*.h) | build/.dir
	$(CC) $(VI_CFLAGS) -c $< -o $@
.PHONY: vi-objects
vi-objects: $(VI_OBJECTS)
build/vi-dev.elf: build/crt0.o $(VI_OBJECTS) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
build/vi-native-check.o: tests/vi-native.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/vi-native.elf: build/crt0.o build/vi-native-check.o build/vi-native-adapter.o build/vi-printf.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
build/vi-native-adapter.o: user/vi/native.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
.PHONY: test-vi-native
test-vi-native: build/nexix-multi.iso build/vi-dev.elf build/vi-native.elf stage
	$(PYTHON) tools/vi_native_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/vi_native_smoke.py $(QEMU) build/nexix-multi.iso 128M

-include $(VI_OBJECTS:.o=.d) build/vi-native-check.d build/vi-native-adapter.d
build/vi-storage.o: tests/vi-storage.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/vi-storage.elf: build/crt0.o build/vi-storage.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-vi-storage
test-vi-storage: build/nexix-multi.iso build/vi-dev.elf build/vi-storage.elf stage
	$(PYTHON) tools/vi_storage_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/vi_storage_smoke.py $(QEMU) build/nexix-multi.iso 128M
.PHONY: test-ex
test-ex: build/nexix-multi.iso build/vi-dev.elf stage
	$(PYTHON) tools/ex_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/ex_smoke.py $(QEMU) build/nexix-multi.iso 128M
.PHONY: test-vi
test-vi: build/nexix-multi.iso build/vi-dev.elf stage
	$(PYTHON) tools/vi_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/vi_smoke.py $(QEMU) build/nexix-multi.iso 128M
build/vi-expreserve.elf: build/crt0.o build/vi-expreserve.o build/vi-directory.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
build/vi-exrecover.elf: build/crt0.o build/vi-exrecover.o build/vi-directory.o build/vi-native.o build/vi-malloc.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
# Recovery uses qsort with the release's private allocator.
LIBC_OBJECTS += build/libc_qsort.o
build/libc.a: build/libc_qsort.o
build/vi-preserve.o: tests/vi-preserve.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/vi-preserve.elf: build/crt0.o build/vi-preserve.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
.PHONY: test-vi-recovery
test-vi-recovery: build/nexix-multi.iso build/vi-dev.elf build/vi-expreserve.elf build/vi-exrecover.elf build/vi-preserve.elf stage
	$(PYTHON) tools/vi_recovery_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/vi_recovery_smoke.py $(QEMU) build/nexix-multi.iso 128M

build/v7-vi.elf build/v7-ex.elf build/v7-view.elf: build/vi-dev.elf
	cp $< $@
build/v7-expreserve.elf: build/vi-expreserve.elf
	cp $< $@
build/v7-exrecover.elf: build/vi-exrecover.elf
	cp $< $@
build/vi-man-vi: reference/ex-vi/050325/vi.1 | build/.dir
	cp $< $@
build/vi-man-ex: reference/ex-vi/050325/ex.1 | build/.dir
	cp $< $@
build/vi-license: reference/ex-vi/050325/LICENSE | build/.dir
	cp $< $@
build/vi-notice: THIRD_PARTY_NOTICES.md | build/.dir
	cp $< $@
build/vi-port.md: docs/vi-port.md | build/.dir
	cp $< $@
-include build/vi-expreserve.d build/vi-exrecover.d build/vi-directory.d build/vi-preserve.d build/vi-storage.d build/termios-guest.d build/termcap-guest.d
.PHONY: test-vi-package
test-vi-package: stage
	$(PYTHON) tools/check_vi_package.py
.PHONY: test-vi-multiuser test-vi-acceptance
test-vi-multiuser: build/nexix-multi.iso rootfs
	$(PYTHON) tools/vi_multiuser_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/vi_multiuser_smoke.py $(QEMU) build/nexix-multi.iso 128M
test-vi-acceptance: test-vi-reference test-termcap test-termcap-output test-termcap-guest test-vga-controls test-termios test-vi-native test-vi-storage test-ex test-vi test-vi-recovery test-vi-package test-vi-multiuser test-clear test-vga test-upgrade test-libc-interfaces test-inventory
test: test-vi-acceptance
.PHONY: test-vi-edit
test-vi-edit: build/nexix-multi.iso stage
	$(PYTHON) tools/vi_edit_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/vi_edit_smoke.py $(QEMU) build/nexix-multi.iso 128M
test-vi-acceptance: test-vi-edit

.PHONY: test-man-pages
test-man-pages:
	$(PYTHON) tests/manual_pages.py
test: test-man-pages

include tools/formatter-tables.mk
include tools/formatter.mk
.PHONY: test-ps
test-ps: build/v7-ps.elf build/nexix-multi.iso test-kinfo-unit
	$(PYTHON) tools/ps_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/ps_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-ps
# Native scheduler load and uptime extension (task 387).
build/loadavg.o: kernel/loadavg.c kernel/include/loadavg.h include/nexix/kinfo.h | build/.dir
	$(CC) $(CFLAGS) -c $< -o $@
OBJECTS += build/loadavg.o
build/nexix.elf: build/loadavg.o
-include build/loadavg.d
build/loadavg-test: tests/loadavg.c kernel/loadavg.c kernel/include/loadavg.h include/nexix/kinfo.h | build/.dir
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include -Iinclude tests/loadavg.c kernel/loadavg.c -o $@
build/v7-uptime.o: user/v7/cmd/uptime.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
.PHONY: test-loadavg-unit
test-loadavg-unit: build/loadavg-test
	./build/loadavg-test
test-unit: test-loadavg-unit
build/uptime-guest.o: tests/uptime-guest.c | build/.dir
	$(CC) $(USER_CFLAGS) -c $< -o $@
build/uptime-guest.elf: build/crt0.o build/uptime-guest.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o build/uptime-guest.o build/libc.a -o $@
.PHONY: test-uptime
test-uptime: test-loadavg-unit test-kinfo-unit build/v7-uptime.elf build/uptime-guest.elf build/nexix-multi.iso
	$(PYTHON) tools/uptime_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/uptime_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-uptime
include tools/equations.mk
.PHONY: test-public-data
test-public-data: build/nexix-multi.iso stage
	$(PYTHON) tools/public_data_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/public_data_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-public-data
.PHONY: test-tc
test-tc: build/v7-tc.elf build/formatter-protocol.elf build/nexix-multi.iso
	$(PYTHON) tools/tc_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/tc_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-tc
