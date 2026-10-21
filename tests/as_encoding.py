"""Compare native x86 encodings and relocations with independent GNU as output."""
from pathlib import Path
import ctypes
import mmap
import os
import random
import struct
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-as';OUT.mkdir(exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
FIXED=['int3','movsb','movsw','movsl','movsq','rep movsb','rep movsq','rep stosb','rep stosq','nop','ret','leave','syscall','ud2','cld','std','cqto','cltq','cltd','pushfq','popfq','fldz','fld1','fchs','fabs','fsqrt','fninit','fwait']
MATRIX=FIXED+[
'movq %rdi,%rax','movq %r15,%r8','movl %ecx,%eax','movw %r9w,%dx','movb %ah,%al','movb %spl,%r8b',
'movq $7,%rax','movq $-1,%r9','movabsq $0x123456789abcdef0,%rax','movl $0xffffffff,%r12d','movb $127,%cl',
'movq %rax,(%rbp)','movq %r8,127(%r13)','movq 128(%rbx),%rax','movl -16(%rbp,%rcx,8),%r9d',
'movl (,%r12,4),%eax','movq symbol(%rip),%r8','movq $symbol,%rax','movabsq $symbol,%r8','movl $symbol,%eax','movq $-2147483648,%r15',
'addq $7,symbol(%rip)','addl $0x12345678,symbol(%rip)','movl $7,symbol(%rip)','leaq symbol(%rip),%rax',
'addq $1,%rax','subq $-128,%r8','andl $255,%eax','orl $0x12345678,%edx','xorl %eax,%eax',
'xchgl %eax,%eax','xchgq %rax,%rax','xchgw %ax,%ax','xchgq %rax,%r8','xchgl %eax,%edx','xchgb %ah,%al','cmpq %r8,%r9','testq %rax,%rax','testl $255,%eax','adcw %ax,(%r8)','sbbq (%r12),%rax',
'imulq %rsi,%rax','imulq $7,%rax','imulq $300,(%r8),%r9','mulq %r8','idivl (%rbp)','negq %rax','notb %ah','incq %r12','decl %eax',
'salq $1,%rax','shrq %cl,%r8','sarl $5,(%r12)','rolw $3,%ax','bsfq %r8,%r9',
'pushq %rbp','pushq %r12','popq %r15','pushq $7','pushq $300','pushq (%r8)','popq (%r8)',
'call symbol','call *%r8','jmp symbol','jmp *(%rax)','je symbol','jge symbol','setne %r8b','cmovgq (%r12),%rax',
'movzbl %al,%eax','movzbq %r8b,%rax','movswq (%rbp),%r9','movslq %eax,%rax',
'movsd %xmm0,%xmm2','movsd (%r8),%xmm9','movss %xmm9,12(%rbp)','movdqu (%r12),%xmm15',
'addsd %xmm1,%xmm0','subss (%rax),%xmm0','mulsd %xmm1,%xmm2','divsd %xmm8,%xmm0','ucomisd %xmm1,%xmm0',
'pxor %xmm15,%xmm15','movq %rax,%xmm0','movq %xmm0,%rax','movq (%rax),%xmm0','movq %xmm0,(%rax)',
'cvtsi2sdq %rax,%xmm0','cvtsi2sd %rax,%xmm0','cvttsd2si %xmm0,%rax','cvttss2sil (%rax),%r8d','cvtss2sd %xmm0,%xmm1',
'flds (%rax)','fldl (%r8)','fldt (%rsp)','fstps (%rax)','fstpl (%r12)','fstpt (%rsp)',
'filds (%rax)','fildl (%rax)','fildq (%rax)','fistpq (%rsp)','fnstcw (%rsp)','fldcw (%rsp)',
'fld %st(1)','fxch %st(1)','fstp %st(1)','fucomip %st(1),%st','faddp %st,%st(1)','fmulp %st,%st(1)',
'fsubp %st,%st(1)','fsubrp %st,%st(1)','fdivp %st,%st(1)','fdivrp %st,%st(1)']
rng=random.Random(182)
for suffix,registers in [('b',['al','cl','spl','r8b','r15b']),('w',['ax','bx','sp','r8w','r15w']),
                         ('l',['eax','ebx','esp','r8d','r15d']),('q',['rax','rbx','rsp','r8','r15'])]:
    for _ in range(35):
        a,b=rng.choices(registers,k=2)
        MATRIX.append(rng.choice(['mov','add','sub','and','or','xor','cmp'])+suffix+' %'+a+',%'+b)
        base=rng.choice(['rax','rbp','rsp','r8','r12','r13']);index=rng.choice(['rcx','r9','r12','r15'])
        disp=rng.choice([0,-128,127,128,-5000,5000]);scale=rng.choice([1,2,4,8])
        MATRIX.append(f'mov{suffix} {disp}(%{base},%{index},{scale}),%{b}')
        imm=rng.choice([-128,-1,1,127])
        MATRIX.append(f'add{suffix} ${imm},%{b}')
def elf(path):
    b=path.read_bytes();shoff=struct.unpack_from('<Q',b,40)[0];n=struct.unpack_from('<H',b,60)[0];si=struct.unpack_from('<H',b,62)[0]
    sections=[struct.unpack_from('<IIQQQQIIQQ',b,shoff+64*i) for i in range(n)]
    names=sections[si];strings=b[names[4]:names[4]+names[5]]
    def data(s):return b[s[4]:s[4]+s[5]]
    text=b'';relocs=[]
    for s in sections:
        name=strings[s[0]:].split(b'\0')[0]
        if name==b'.text':text=data(s)
        if name==b'.rela.text':
            syms=sections[s[6]];st=sections[syms[6]];sn=data(st)
            for off in range(0,s[5],24):
                pos,info,add=struct.unpack_from('<QQq',data(s),off);sym=struct.unpack_from('<IBBHQQ',data(syms),(info>>32)*24)
                relocs.append((pos,info&0xffffffff,add,sn[sym[0]:].split(b'\0')[0].decode(),'-'))
    return text,relocs
with tempfile.TemporaryDirectory(prefix='nx-encoding-') as temp:
    folder=Path(temp);binary=folder/'encode'
    subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/as','user/as/parse.c','user/as/encode.c','tests/as-encode.c','-o',str(binary)],cwd=ROOT,check=True)
    def run(source,ok=True):
        (folder/'input.s').write_text(source+'\n')
        r=subprocess.run([str(binary),str(folder/'input.s')],capture_output=True,text=True,env=ENV,timeout=5)
        assert r.returncode>=0 and 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,r
        if ok:assert r.returncode==0,(source,r)
        return r
    alloutput=[]
    for source in MATRIX:
        r=run(source);subprocess.run(['as','--64',str(folder/'input.s'),'-o',str(folder/'oracle.o')],capture_output=True,check=True)
        expected,relocs=elf(folder/'oracle.o');lines=r.stdout.splitlines();actual=bytes.fromhex(lines[0])
        assert actual==expected,(source,actual.hex(),expected.hex())
        actualrel=[]
        for line in lines[1:]:
            _,off,kind,add,plus,minus=line.split();actualrel.append((int(off),int(kind),int(add),plus,minus))
        assert actualrel==relocs,(source,actualrel,relocs)
        alloutput.append(r.stdout)
    for source in ['movb $symbol,%al','movw $symbol,%ax','movq %ah,%rax','movb %ah,%r8b','movq (%rsp,%rsp,4),%rax','movq %notreg,%rax',
                   'movq $0x123456789abcdef0,(%rax)','call *%xmm0','pushl %eax','movq %eax,%rax','salq $300,%rax','movq 0x123456789(%rip),%rax','cvttss2sil %xmm0,%rax','unknown %rax']:
        assert run(source,ok=False).returncode!=0,source
    # Execute independently checked machine code under the host System V ABI.
    integer='movq %rdi,%rax\nimulq %rsi,%rax\naddq $7,%rax\nret'
    floating='movsd %xmm0,%xmm2\nmulsd %xmm1,%xmm2\naddsd %xmm0,%xmm2\nmovsd %xmm2,%xmm0\nret'
    codebytes=[]
    for source,restype,argtypes,cases in [(integer,ctypes.c_long,[ctypes.c_long]*2,[(3,4,19),(-7,5,-28)]),
                                         (floating,ctypes.c_double,[ctypes.c_double]*2,[(1.5,2.0,4.5),(-2.0,3.0,-8.0)])]:
        encoded=b''.join(bytes.fromhex(line) for line in run(source).stdout.splitlines())
        mem=mmap.mmap(-1,len(encoded),prot=mmap.PROT_READ|mmap.PROT_WRITE|mmap.PROT_EXEC);mem.write(encoded)
        address=ctypes.addressof(ctypes.c_char.from_buffer(mem));fn=ctypes.CFUNCTYPE(restype,*argtypes)(address)
        for a,b,answer in cases:assert fn(a,b)==answer
        del fn;mem.close();codebytes.append(encoded)
    (OUT/'encoding.s').write_text('\n'.join(MATRIX)+'\n')
    (OUT/'encoding.out').write_text(''.join(alloutput))
    (OUT/'encoded.S').write_text('.text\n.globl encoded_integer\nencoded_integer:\n.byte '+','.join(str(x) for x in codebytes[0])+'\n.globl encoded_float\nencoded_float:\n.byte '+','.join(str(x) for x in codebytes[1])+'\n.section .note.GNU-stack,"",@progbits\n')
print('PASS: x86_64 encoding production sanitizers, GNU byte/relocation oracle, invalid operands and integer/SSE machine-code execution')
