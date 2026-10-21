"""Native ELF producer acceptance: inspection, independent link and execution."""
from pathlib import Path
import os
import struct
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-as';OUT.mkdir(exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
SOURCE='''.text
.globl native_function
.type native_function,@function
native_function:
 movq %rdi,%rax
 addq storage(%rip),%rax
 movq %rax,%rdi
 subq $8,%rsp
 call helper
 addq $8,%rsp
 ret
.size native_function,.-native_function
.data
.globl storage
.type storage,@object
storage: .quad 9
.size storage,8
.bss
.align 16
.globl zeroes
zeroes: .zero 32
.comm common,8,8
.lcomm localbits,4
.local local_more
.comm local_more,8,8
.section .rodata,"a",@progbits
message: .asciz "native object"
.data
.globl pointer
pointer: .quad storage+4
.long -1
.long helper-.
.set alias,storage
.globl alias
.weak weak_missing
.quad weak_missing
.section .note.GNU-stack,"",@progbits
'''
DRIVER='''long helper(long n){return n*3;} extern long native_function(long),storage,common,alias; extern void *pointer; extern char zeroes[32]; int main(void){int i;if(native_function(5)!=42||storage!=9||alias!=9||pointer!=(char*)&storage+4||common)return 1;for(i=0;i<32;i++)if(zeroes[i])return 2;return 0;}\n'''
def material_sections(path):
    b=path.read_bytes();off=struct.unpack_from('<Q',b,40)[0];count=struct.unpack_from('<H',b,60)[0];names_index=struct.unpack_from('<H',b,62)[0]
    headers=[struct.unpack_from('<IIQQQQIIQQ',b,off+64*i) for i in range(count)]
    n=headers[names_index];names=b[n[4]:n[4]+n[5]];out={}
    for h in headers:
        name=names[h[0]:].split(b'\0')[0].decode()
        if name in ('.text','.data','.rodata','.bss'):
            out[name]=(h[1],h[2],h[5],h[8],b[h[4]:h[4]+h[5]] if h[1]!=8 else b'')
    return out
with tempfile.TemporaryDirectory(prefix='nx-as-object-') as temp:
    folder=Path(temp);binary=folder/'as-object'
    subprocess.run(['cc','-D_POSIX_C_SOURCE=200809L','-std=c11','-g','-O1','-fsanitize=address,undefined','-fno-pie','-no-pie','-Wall','-Wextra','-Werror','-Iuser/as','-Iuser/toolchain','user/as/parse.c','user/as/encode.c','user/as/object.c','user/toolchain/archive.c','user/toolchain/object.c','tests/as-object.c','-o',str(binary)],cwd=ROOT,check=True)
    def run(source,ok=True):
        (folder/'input.s').write_text(source)
        r=subprocess.run([str(binary),str(folder/'input.s'),str(folder/'output.o')],capture_output=True,env=ENV,timeout=10)
        assert r.returncode>=0 and b'AddressSanitizer' not in r.stderr and b'runtime error:' not in r.stderr,r
        if ok:assert r.returncode==0,r
        return r
    run(SOURCE);first=(folder/'output.o').read_bytes()
    subprocess.run(['as','--64',str(folder/'input.s'),'-o',str(folder/'oracle.o')],check=True)
    assert material_sections(folder/'output.o')==material_sections(folder/'oracle.o')
    h=struct.unpack_from('<16sHHIQQQIHHHHHH',first)
    assert h[1:4]==(1,62,1) and h[8:]==(64,0,0,64,h[12],h[13]),h
    assert h[6]+h[12]*64==len(first)
    result=subprocess.run(['readelf','-a',str(folder/'output.o')],capture_output=True,text=True,check=True)
    assert not result.stderr and all(x in result.stdout for x in ['R_X86_64_PC32','R_X86_64_PLT32','R_X86_64_64','COM common','WEAK','alias']),result
    (folder/'main.c').write_text(DRIVER)
    subprocess.run(['cc','-no-pie',str(folder/'main.c'),str(folder/'output.o'),'-o',str(folder/'app')],check=True)
    subprocess.run([str(folder/'app')],check=True)
    run(SOURCE);assert (folder/'output.o').read_bytes()==first
    (OUT/'object.s').write_text(SOURCE);(OUT/'object.o').write_bytes(first);(OUT/'object-main.c').write_text(DRIVER)
    # Forward labels, current location, absolute aliases and local branch resolution.
    run('.text\n.globl f\nf: jmp 1f\n.byte 1,2,3\n1: movl $7,%eax\nret\n.size f,.-f\n.set CONST,17\n.data\n.quad f\n.long CONST\n.long .-.\n')
    subprocess.run(['readelf','-a',str(folder/'output.o')],check=True,stdout=subprocess.DEVNULL)
    for source in ['.text\nf: ret\nf: ret\n','.data\n.quad .Lmissing\n',
                   '.set a,b\n.set b,a\n','.bss\n.quad external\n','.bss\n.byte 1\n',
                   '.text\nmovb $external,%al\n','.data\n.long a-b\n',
                   '.set a,b0\n'+''.join('.set b'+str(i)+',b'+str(i+1)+'\n' for i in range(200))+'.set b200,1\n',
                   '.text\n.zero 8388608\nret\n']:
        saved=(folder/'output.o').read_bytes();assert run(source,ok=False).returncode!=0,source[:80]
        assert (folder/'output.o').read_bytes()==saved
print('PASS: native ELF64 production sanitizers, section/symbol/relocation inspection, independent link/execution, repeatability and failed-output preservation')
