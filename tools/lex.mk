# Native lex keeps the supplied generator; its grammar is bootstrapped by host V7 yacc.
LEX_NATIVE_OBJECTS = build/lex-parser.o $(addprefix build/lex-,$(addsuffix .o,lmain sub1 sub2 header))
build/gen-lex/parser.c: build/host-v7-yacc user/lex/parser.y rootfs/usr/lib/yaccpar
	mkdir -p build/gen-lex
	cd build/gen-lex && NX_YACCPAR=../../rootfs/usr/lib/yaccpar ../host-v7-yacc ../../user/lex/parser.y && mv y.tab.c parser.c
build/lex-parser.o: build/gen-lex/parser.c user/lex/ldefs.c | build/.dir
	$(CC) $(V7_CFLAGS) -Iuser/lex -c $< -o $@
build/lex-%.o: user/lex/%.c user/lex/ldefs.c user/lex/once.c | build/.dir
	$(CC) $(V7_CFLAGS) -c $< -o $@
build/v7-lex.elf: build/crt0.o $(LEX_NATIVE_OBJECTS) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
-include $(LEX_NATIVE_OBJECTS:.o=.d)
.PHONY: test-lex-unit test-lex
test-lex-unit: build/gen-lex/parser.c
	$(PYTHON) tests/lex.py
test-lex: test-lex-unit multi-iso rootfs
	$(PYTHON) tools/lex_smoke.py $(QEMU) build/nexix-multi.iso 16M
	$(PYTHON) tools/lex_smoke.py $(QEMU) build/nexix-multi.iso 128M
test: test-lex
test-unit: test-lex-unit

# Private acceptance scanners compiled from the exact generated C compared in guests.
build/lex-scanner.o: build/fixtures-lex/lex.yy.c | build/.dir
	$(CC) $(V7_CFLAGS) -w -c $< -o $@
build/lex-table.o: build/fixtures-lex/table.c | build/.dir
	$(CC) $(V7_CFLAGS) -w -c $< -o $@
build/lex-scanner.elf build/lex-table.elf: build/lex-%.elf: build/crt0.o build/lex-%.o build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld $(filter %.o %.a,$^) -o $@
# These files are emitted by the sanitizer acceptance generator.
build/fixtures-lex/lex.yy.c build/fixtures-lex/table.c: ;
