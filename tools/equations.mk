# Bootstrap parser generation uses the retained V7 yacc sources on the host.
build/host-v7-yacc: $(wildcard tools/v7-yacc/*) | build/.dir
	$(HOST_CC) -std=gnu89 -w -fcommon tools/v7-yacc/y1.c tools/v7-yacc/y2.c tools/v7-yacc/y3.c tools/v7-yacc/y4.c -o $@
build/gen-eqn/e.c build/gen-eqn/e.def &: build/host-v7-yacc user/eqn/e.y tools/eqn_generate.py
	$(PYTHON) tools/eqn_generate.py eqn
build/gen-neqn/e.c build/gen-neqn/e.def &: build/host-v7-yacc user/neqn/e.y tools/eqn_generate.py
	$(PYTHON) tools/eqn_generate.py neqn
EQN_NAMES = e diacrit eqnbox font fromto funny glob integral io lex lookup mark matrix move over paren pile shift size sqrt text
EQN_OBJECTS = $(addprefix build/eqn-,$(addsuffix .o,$(EQN_NAMES)))
NEQN_OBJECTS = $(addprefix build/neqn-,$(addsuffix .o,$(EQN_NAMES)))
build/eqn-e.o: build/gen-eqn/e.c user/eqn/e.h
	$(CC) $(V7_CFLAGS) -Iuser/eqn -Ibuild/gen-eqn -c $< -o $@
build/neqn-e.o: build/gen-neqn/e.c user/neqn/e.h
	$(CC) $(V7_CFLAGS) -DNEQN -Iuser/neqn -Ibuild/gen-neqn -c $< -o $@
build/eqn-%.o: user/eqn/%.c user/eqn/e.h build/gen-eqn/e.def
	$(CC) $(V7_CFLAGS) -Iuser/eqn -Ibuild/gen-eqn -c $< -o $@
build/neqn-%.o: user/neqn/%.c user/neqn/e.h build/gen-neqn/e.def
	$(CC) $(V7_CFLAGS) -DNEQN -Iuser/neqn -Ibuild/gen-neqn -c $< -o $@
build/v7-eqn.elf: build/crt0.o $(EQN_OBJECTS) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o $(EQN_OBJECTS) build/libc.a -o $@
build/v7-neqn.elf: build/crt0.o $(NEQN_OBJECTS) build/libc.a user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user/user.ld build/crt0.o $(NEQN_OBJECTS) build/libc.a -o $@
-include $(EQN_OBJECTS:.o=.d) $(NEQN_OBJECTS:.o=.d)
.PHONY: test-eqn test-neqn
test-eqn: build/v7-eqn.elf build/v7-troff.elf build/nexix-multi.iso
	$(PYTHON) tools/equations_smoke.py $(QEMU) build/nexix-multi.iso 16M eqn
	$(PYTHON) tools/equations_smoke.py $(QEMU) build/nexix-multi.iso 128M eqn
test-neqn: build/v7-neqn.elf build/v7-troff.elf build/nexix-multi.iso
	$(PYTHON) tools/equations_smoke.py $(QEMU) build/nexix-multi.iso 16M neqn
	$(PYTHON) tools/equations_smoke.py $(QEMU) build/nexix-multi.iso 128M neqn
test: test-eqn test-neqn
