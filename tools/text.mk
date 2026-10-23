# Tasks 140-164: retained V7 text commands. Each has an ASan/UBSan host
# fixture (tests/<name>.py) and installed-command guest acceptance.

# egrep's expression parser comes from the retained V7 yacc, as in cmake.
build/gen-egrep/y.tab.c: user/v7/cmd/egrep.y build/host-nx-yacc rootfs/usr/lib/yaccpar
	mkdir -p build/gen-egrep
	cp user/v7/cmd/egrep.y build/gen-egrep/egrep.y
	cd build/gen-egrep && NX_YACCPAR=$(CURDIR)/rootfs/usr/lib/yaccpar $(CURDIR)/build/host-nx-yacc egrep.y >/dev/null
build/v7-egrep.o: build/gen-egrep/y.tab.c user/v7/compat.h | build/.dir
	$(CC) $(V7_CFLAGS) -include user/v7/compat.h -c $< -o $@

# sed keeps the supplied two-file layout.
V7_SED_OBJECTS = build/v7-sed-sed0.o build/v7-sed-sed1.o
-include $(V7_SED_OBJECTS:.o=.d)
build/v7-sed-%.o: user/v7/cmd/sed/%.c user/v7/cmd/sed/sed.h user/v7/compat.h | build/.dir
	$(CC) $(V7_CFLAGS) -include user/v7/compat.h -c $< -o $@
build/v7-sed.elf: build/crt0.o $(V7_SED_OBJECTS) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o $(V7_SED_OBJECTS) build/libc.a -o $@
.PHONY: test-col-unit test-col
test-col-unit:
	$(PYTHON) tests/col.py
test-unit: test-col-unit
test-col: test-col-unit multi-iso rootfs
	$(PYTHON) tools/col_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/col_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-col
.PHONY: test-comm-unit test-comm
test-comm-unit:
	$(PYTHON) tests/comm.py
test-unit: test-comm-unit
test-comm: test-comm-unit multi-iso rootfs
	$(PYTHON) tools/comm_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/comm_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-comm
.PHONY: test-join-unit test-join
test-join-unit:
	$(PYTHON) tests/join.py
test-unit: test-join-unit
test-join: test-join-unit multi-iso rootfs
	$(PYTHON) tools/join_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/join_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-join
.PHONY: test-look-unit test-look
test-look-unit:
	$(PYTHON) tests/look.py
test-unit: test-look-unit
test-look: test-look-unit multi-iso rootfs
	$(PYTHON) tools/look_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/look_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-look
.PHONY: test-rev-unit test-rev
test-rev-unit:
	$(PYTHON) tests/rev.py
test-unit: test-rev-unit
test-rev: test-rev-unit multi-iso rootfs
	$(PYTHON) tools/rev_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/rev_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-rev
.PHONY: test-sp-unit test-sp
test-sp-unit:
	$(PYTHON) tests/sp.py
test-unit: test-sp-unit
test-sp: test-sp-unit multi-iso rootfs
	$(PYTHON) tools/sp_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/sp_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-sp
.PHONY: test-split-unit test-split
test-split-unit:
	$(PYTHON) tests/split.py
test-unit: test-split-unit
test-split: test-split-unit multi-iso rootfs
	$(PYTHON) tools/split_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/split_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-split
.PHONY: test-tabs-unit test-tabs
test-tabs-unit:
	$(PYTHON) tests/tabs.py
test-unit: test-tabs-unit
test-tabs: test-tabs-unit multi-iso rootfs
	$(PYTHON) tools/tabs_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/tabs_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-tabs
.PHONY: test-tee-unit test-tee
test-tee-unit:
	$(PYTHON) tests/tee.py
test-unit: test-tee-unit
test-tee: test-tee-unit multi-iso rootfs
	$(PYTHON) tools/tee_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/tee_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-tee
.PHONY: test-tr-unit test-tr
test-tr-unit:
	$(PYTHON) tests/tr.py
test-unit: test-tr-unit
test-tr: test-tr-unit multi-iso rootfs
	$(PYTHON) tools/tr_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/tr_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-tr
.PHONY: test-tsort-unit test-tsort
test-tsort-unit:
	$(PYTHON) tests/tsort.py
test-unit: test-tsort-unit
test-tsort: test-tsort-unit multi-iso rootfs
	$(PYTHON) tools/tsort_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/tsort_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-tsort
.PHONY: test-uniq-unit test-uniq
test-uniq-unit:
	$(PYTHON) tests/uniq.py
test-unit: test-uniq-unit
test-uniq: test-uniq-unit multi-iso rootfs
	$(PYTHON) tools/uniq_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/uniq_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-uniq
.PHONY: test-egrep-unit test-egrep
test-egrep-unit:
	$(PYTHON) tests/egrep.py
test-unit: test-egrep-unit
test-egrep: test-egrep-unit multi-iso rootfs
	$(PYTHON) tools/egrep_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/egrep_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-egrep
.PHONY: test-fgrep-unit test-fgrep
test-fgrep-unit:
	$(PYTHON) tests/fgrep.py
test-unit: test-fgrep-unit
test-fgrep: test-fgrep-unit multi-iso rootfs
	$(PYTHON) tools/fgrep_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/fgrep_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-fgrep
.PHONY: test-pr-unit test-pr
test-pr-unit:
	$(PYTHON) tests/pr.py
test-unit: test-pr-unit
test-pr: test-pr-unit multi-iso rootfs
	$(PYTHON) tools/pr_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/pr_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-pr
.PHONY: test-sed-unit test-sed
test-sed-unit:
	$(PYTHON) tests/sed.py
test-unit: test-sed-unit
test-sed: test-sed-unit multi-iso rootfs
	$(PYTHON) tools/sed_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/sed_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-sed
