"""Static ELF64 producer: independent relocation decoding, execution and failures."""
from pathlib import Path
import os
import random
import struct
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-ld';OUT.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
def run(*args,**kw):return subprocess.run(args,check=True,**kw)
def sections(b):
    start=struct.unpack_from('<Q',b,40)[0];n=struct.unpack_from('<H',b,60)[0];ni=struct.unpack_from('<H',b,62)[0]
    hs=[struct.unpack_from('<IIQQQQIIQQ',b,start+64*i) for i in range(n)]
    h=hs[ni];names=b[h[4]:h[4]+h[5]]
    return {names[h[0]:].split(b'\0')[0].decode():h for h in hs}
with tempfile.TemporaryDirectory(prefix='nx-ld-output-') as tmp:
    binary=Path(tmp)/'output'
    run('cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',
        '-Iuser/ld','-Iuser/toolchain','user/ld/resolve.c','user/ld/output.c','user/toolchain/object.c',
        'user/toolchain/archive.c','tests/ld-output.c','-o',str(binary),cwd=ROOT)
    def asm(name,text):
        (OUT/(name+'.s')).write_text(text)
        run('as','--64',str(OUT/(name+'.s')),'-o',str(OUT/(name+'.o')))
    def check(entry,output,inputs,ok=True,message=None):
        p=subprocess.run([str(binary),entry,str(OUT/output),*[str(ROOT/i) if '/' in i else str(OUT/i) for i in inputs]],capture_output=True,env=ENV,timeout=10)
        assert p.returncode==(0 if ok else 1),(inputs,p)
        assert b'AddressSanitizer' not in p.stderr and b'runtime error:' not in p.stderr,p
        if message:assert message.encode() in p.stderr,p
        return p
    asm('hoststart','.globl _start\n.text\n_start: call calculate\n movl %eax,%edi\n movl $60,%eax\n syscall\n')
    asm('calculate','.globl calculate\n.text\ncalculate: movq pointer(%rip),%rax\n movl (%rax),%eax\n subl $42,%eax\n ret\n.data\n.globl pointer\npointer: .quad number\n.globl number\nnumber: .long 42\n.bss\n.balign 64\nzero: .zero 64\n.comm common,24,32\n')
    check('_start','host.elf',['hoststart.o','calculate.o'])
    run(str(OUT/'host.elf'))
    data=(OUT/'host.elf').read_bytes();hs=sections(data)
    assert data[:9]==b'\x7fELF\x02\x01\x01\x00\x00'
    assert struct.unpack_from('<HHI',data,16)==(2,62,1)
    assert hs['.text'][2]==6 and hs['.data'][2]==3 and hs['.bss'][1]==8
    ph=[struct.unpack_from('<IIQQQQQQ',data,64+i*56) for i in range(3)]
    assert ph[0][0:2]==(1,5) and ph[1][0:2]==(1,6) and ph[2][0:2]==(0x6474e551,6)
    assert ph[1][6]>ph[1][5] and ph[0][3]+ph[0][6]<=ph[1][3]
    # Exact relocated data value, decoded without the producer's own reader.
    d=hs['.data'];assert struct.unpack_from('<Q',data,d[4])[0]==d[3]+8
    # All required relocation forms and signed/unsigned limits with an absolute symbol.
    asm('relocs','.globl _start\n.text\n_start: ret\n.data\n.quad target+7\n.long target+3\n.long target-.\nmovq $target,%rax\ncall target\n')
    asm('target','.globl target\n.set target,0x12345678\n')
    check('_start','relocs.elf',['relocs.o','target.o'])
    b=(OUT/'relocs.elf').read_bytes();d=sections(b)['.data'];v=b[d[4]:d[4]+d[5]]
    assert struct.unpack_from('<Q',v,0)[0]==0x1234567f
    assert struct.unpack_from('<I',v,8)[0]==0x1234567b
    assert struct.unpack_from('<i',v,12)[0]==0x12345678-(d[3]+12)
    assert struct.unpack_from('<i',v,19)[0]==0x12345678
    assert struct.unpack_from('<i',v,24)[0]==0x12345678-(d[3]+28)
    # GNU relocation bytes for the same explicitly fixed addresses.
    script=Path(tmp)/'layout.ld';script.write_text('ENTRY(_start) SECTIONS { . = 0x40000000; .text : { *(.text) } . = 0x40001000; .data : { *(.data) } }')
    oracle=Path(tmp)/'oracle';run('ld','-T',str(script),'-o',str(oracle),str(OUT/'relocs.o'),str(OUT/'target.o'))
    b=oracle.read_bytes();d=sections(b)['.data'];assert b[d[4]:d[4]+d[5]]==v
    asm('negative','.globl target\n.set target,-1\n')
    asm('signed','.globl _start\n.text\n_start: movq $target,%rax\nret\n')
    check('_start','signed.elf',['signed.o','negative.o'])
    asm('wide','.globl target\n.set target,0x100000000\n')
    check('_start','preserve',['relocs.o','wide.o'],False,'relocation overflow')
    asm('unsigned','.globl _start\n.text\n_start: ret\n.data\n.long target\n')
    check('_start','preserve',['unsigned.o','negative.o'],False,'relocation overflow')
    # Weak undefineds resolve to zero without extracting an archive.
    asm('weakzero','.globl _start\n.weak missing\n.text\n_start: ret\n.data\n.quad missing\n')
    check('_start','weakzero.elf',['weakzero.o'])
    b=(OUT/'weakzero.elf').read_bytes();h=sections(b)['.data'];assert b[h[4]:h[4]+8]==bytes(8)
    # Compile a genuine native libc consumer with the bootstrap compiler; link it here.
    source=OUT/'native.c';source.write_text('#include <stdio.h>\n#include <stdlib.h>\nlong zero[32]; extern char __nx_text_end[]; int main(void){for(int i=0;i<32;i++)if(zero[i])return 1;if(!__nx_text_end)return 2;puts("PASS: native linked C and libc");return 0;}\n')
    run('gcc','-std=c11','-O2','-fno-pie','-fno-stack-protector','-fno-asynchronous-unwind-tables','-mno-red-zone','-mno-avx','-ffreestanding','-fno-builtin','-nostdinc','-Ilibc/include','-Iinclude','-c',str(source),'-o',str(OUT/'native.o'),cwd=ROOT)
    check('_start','native.elf',['build/crt0.o','native.o','build/libc.a'])
    # Failure leaves an existing output untouched.
    (OUT/'preserve').write_bytes(b'original')
    asm('huge','.globl _start\n.text\n_start: ret\n.bss\n.zero 8388608\n')
    check('_start','preserve',['huge.o'],False,'8 MiB')
    check('absent','preserve',['hoststart.o','calculate.o'],False,'entry')
    check('_start','preserve',['hoststart.o'],False,'undefined')
    assert (OUT/'preserve').read_bytes()==b'original'
    # Corrupt valid RELA tables: unsupported type, bad symbol, offset, target and width.
    original=(OUT/'relocs.o').read_bytes();start=struct.unpack_from('<Q',original,40)[0];count=struct.unpack_from('<H',original,60)[0]
    rel=next(start+64*i for i in range(count) if struct.unpack_from('<I',original,start+64*i+4)[0]==4)
    offset=struct.unpack_from('<Q',original,rel+24)[0]
    for off,fmt,val in [(offset+8,'Q',(1<<32)|99),(offset,'Q',0xffffffffffffffff),
                        (offset+8,'Q',(99999<<32)|1),(rel+44,'I',65535),(rel+56,'Q',16)]:
        b=bytearray(original);struct.pack_into('<'+fmt,b,off,val);(OUT/'broken.o').write_bytes(b)
        check('_start','preserve',['broken.o','target.o'],False)
    rng=random.Random(186)
    for _ in range(100):
        b=bytearray(original)
        for _ in range(rng.randrange(1,4)):b[rng.randrange(len(b))]=rng.randrange(256)
        (OUT/'fuzz.o').write_bytes(b)
        p=subprocess.run([str(binary),'_start',str(OUT/'fuzz.elf'),str(OUT/'fuzz.o'),str(OUT/'target.o')],capture_output=True,env=ENV,timeout=5)
        assert p.returncode in (0,1) and b'AddressSanitizer' not in p.stderr and b'runtime error:' not in p.stderr,p
print('PASS: ld ELF layout, exact GNU relocation oracle, execution, native C/libc link, overflow and malformed-input sanitizers')
