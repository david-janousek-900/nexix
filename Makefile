TOOLCHAIN_PREFIX ?= x86_64-elf-
CC = $(TOOLCHAIN_PREFIX)gcc
LD = $(TOOLCHAIN_PREFIX)ld
PYTHON ?= python3
QEMU ?= qemu-system-x86_64
CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -fno-pie -m64 -mno-red-zone -mgeneral-regs-only -fno-asynchronous-unwind-tables -Ikernel/include -Iinclude -MMD -MP
OBJECTS = build/boot.o build/main.o build/serial.o build/interrupts.o build/interrupt_stubs.o build/boot_info.o build/memory.o build/paging.o build/paging_test.o build/segments.o build/user_entry.o build/user_blob.o build/user_test.o build/process.o build/context.o build/process_test.o build/process_blob.o build/uaccess.o build/syscall.o build/syscall_test.o build/syscall_blob.o
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
run: iso
	$(QEMU) -machine pc -accel tcg -cpu qemu64 -smp 1 -m 128M -cdrom build/nexix.iso -display none -serial stdio -monitor none -no-reboot

smoke: iso
	$(PYTHON) tools/smoke.py $(QEMU) build/nexix.iso

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

test-unit: build/memory-test build/v7-types-test build/uaccess-test
	./build/memory-test
	./build/v7-types-test
	./build/uaccess-test

test: test-unit smoke build/test-ud.iso build/test-pf.iso build/test-memory.iso test-paging test-user test-syscall
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
	$(PYTHON) -c 'import shutil; shutil.rmtree("build", ignore_errors=True)'

.PHONY: test-process
test-process: iso
	$(PYTHON) tools/smoke.py $(QEMU) build/nexix.iso process 16M

.PHONY: test-syscall
test-syscall: iso
	$(PYTHON) tools/smoke.py $(QEMU) build/nexix.iso syscall 16M
