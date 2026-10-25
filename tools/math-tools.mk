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
