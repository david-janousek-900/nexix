"""Assembler first-pass acceptance with independent GNU syntax validation."""
from pathlib import Path
import os
import random
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-as';OUT.mkdir(exist_ok=True)
SOURCE='''.file "frontend.s"
.text
.globl entry
.type entry,@function
entry:
 pushq %rbp
 movq %rsp,%rbp
 leaq message(%rip),%rdi
 movq -16(%rbp,%rcx,8),%rdx
 movl $7,%esi
 call external
 jmp 1f
1: popq %rbp
 ret
.size entry,.-entry
.section .rodata,"a",@progbits
message: .asciz "hello\\n"
.data
.quad entry+4
.long 2*(3+4)
.bss
.align 8
storage: .zero 32
.comm common,8,8
.local private
.lcomm private,4
.data
.float 1.5
.double 1.5
.quad (1<<62)|0xff
.set ALIGNMENT,16
.balign ALIGNMENT
.weak external
'''
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
with tempfile.TemporaryDirectory(prefix='nx-as-') as temp:
    folder=Path(temp);binary=folder/'parse'
    subprocess.run(['cc','-std=c11','-g','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/as','user/as/parse.c','tests/as-parse.c','-o',str(binary)],cwd=ROOT,check=True)
    def run(text):
        (folder/'input.s').write_text(text)
        r=subprocess.run([str(binary),str(folder/'input.s')],capture_output=True,timeout=5,env=ENV)
        assert r.returncode>=0 and b'AddressSanitizer' not in r.stderr and b'runtime error:' not in r.stderr,r
        return r
    r=run(SOURCE);assert r.returncode==0 and r.stdout==b'instructions=9 labels=4 data=5 memory=2\n',r
    subprocess.run(['as','--64',str(folder/'input.s'),'-o',str(folder/'oracle.o')],check=True)
    symbols=subprocess.check_output(['readelf','-s',str(folder/'oracle.o')]);assert b'entry' in symbols and b'common' in symbols
    (OUT/'frontend.s').write_text(SOURCE)
    (OUT/'expected').write_bytes(r.stdout)
    for text in ['x:\nx:\n','.quad 1/0\n','.long 1<<64\n','.quad '+('('*200)+'1'+(')'*200)+'\n',
                 '.ascii "unfinished\n','.quad 0xgg\n','.space 999999999999\n',
                 'movq 0(%rax,%rbx,3),%rax\n','movq %rax,%rbx,%rcx,%rdx\n',
                 '.globl '+('X'*300)+'\n','.section bad,"!"\n','jmp 1b\n','movq %rax,%rbx\x00ret\n',
                 '.comm x,4,3\n','.unknown\n']:
        r=run(text);assert r.returncode==1 and r.stderr,text
    rng=random.Random(181)
    for _ in range(300):run(''.join(rng.choice('abc012()$%#,:;+-*/|&~<>"\\\n ') for _ in range(rng.randrange(1,500))))
print('PASS: assembler parser ASan/UBSan, GNU-accepted syntax, labels/symbols, typed operands/directives, malformed expressions and limits')
