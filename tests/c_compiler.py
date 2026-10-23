"""Retained V7 C compiler passes: sanitizers, GCC oracle, native assembler, headers and V7 sources."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-c';OUT.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
SAN=['-g','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=undefined','-fno-pie','-no-pie','-w']
C0=['c00','c01','c02','c03','c04','c05']
C1=['c10','c11','c12','c13']
DIALECT='5 3 52 1\n-1 1 7 3\n1 -1\n5 4 8\n'
# V7 commands whose supplied sources the native passes must compile and
# assemble against the installed headers (others need absent PDP-11
# interfaces, non-constant stdio initializers or structure values).
V7_SOURCES='''ac accton at atrun basename cal calendar cat cb cc checkeq chgrp chmod chown cmp col cp cron crypt
cu date dd deroff diff3 diff diffh du echo ed fgrep file find getty grep init iostat join kill ld ln
login look ls mail makekey mesg mkdir mknod mount mv newgrp nice od osh passwd pr pwd random rev rm
rmdir sa sleep sp spline split stty su sum sync tabs tail tc tee test time tk touch tr tsort tty
umount uniq units update wall wc who write yes'''.split()

def run(cmd,**kw):
    r=subprocess.run(cmd,capture_output=True,env=ENV,timeout=60,**kw)
    assert b'Sanitizer' not in r.stderr and b'runtime error:' not in r.stderr,(cmd,r.stderr[-3000:])
    return r

with tempfile.TemporaryDirectory(prefix='nx-c-') as tmp:
    d=Path(tmp)
    c0,c1,cvopt,nas=d/'c0',d/'c1',d/'cvopt',d/'as2'
    subprocess.run(['cc','-std=gnu99',*SAN,*[f'user/c/{n}.c' for n in C0],'-o',str(c0)],cwd=ROOT,check=True)
    subprocess.run(['cc','-std=c11',*SAN,'user/c/cvopt.c','-o',str(cvopt)],cwd=ROOT,check=True)
    table=d/'table.s'
    r=run([str(cvopt),'user/c/table.s',str(table)],cwd=ROOT)
    assert r.returncode==0,r.stderr
    # The bootstrap build consumes the same generated table.
    subprocess.run(['cc','-std=gnu99',*SAN,*[f'user/c/{n}.c' for n in C1],str(table),'-o',str(c1)],cwd=ROOT,check=True)
    subprocess.run(['cc','-D_POSIX_C_SOURCE=200809L','-std=c11','-O1','-w','-Iuser/as','-Iuser/toolchain','-DAS_WORKER',
        'user/v7/cmd/as.c','user/as/parse.c','user/as/encode.c','user/as/object.c','user/toolchain/archive.c','user/toolchain/object.c','-o',str(nas)],cwd=ROOT,check=True)
    c2=d/'c2'
    subprocess.run(['cc','-std=gnu99',*SAN,'user/c/c20.c','user/c/c21.c','-o',str(c2)],cwd=ROOT,check=True)
    def optimize(asm):
        out=asm.with_suffix('.opt.s')
        r=run([str(c2),str(asm),str(out)]);assert r.returncode==0,r.stderr
        before,after=len(asm.read_text().splitlines()),len(out.read_text().splitlines())
        assert after<before,(asm,before,after)
        return out
    # cvopt reports malformed tables instead of emitting partial data.
    bad=d/'bad.s';bad.write_text('cr1:\n%q,n\n\tF\n')
    r=run([str(cvopt),str(bad),str(d/'bad.i')]);assert r.returncode!=0
    bad.write_text('cr1:\n%n,n\n\n');r=run([str(cvopt),str(bad),str(d/'bad.i')]);assert r.returncode!=0

    def compile(source,name,expect_ok=True):
        r0=run([str(c0),str(source),str(d/(name+'.1')),str(d/(name+'.2'))])
        if not expect_ok and r0.returncode:
            return None
        assert r0.returncode==0,(name,r0.stderr)
        r1=run([str(c1),str(d/(name+'.1')),str(d/(name+'.2')),str(d/(name+'.s'))])
        assert r1.returncode==0,(name,r1.stderr)
        return d/(name+'.s')

    def execute(asm,name,assembler):
        obj=d/(name+'.o')
        if assembler=='gnu':
            subprocess.run(['as','--64',str(asm),'-o',str(obj)],check=True)
        else:
            r=run([str(nas),'-o',str(obj),str(asm)]);assert r.returncode==0,r.stderr
        subprocess.run(['cc','-no-pie',str(obj),'-o',str(d/name)],check=True)
        return subprocess.run([str(d/name)],capture_output=True,timeout=20)

    fixture=ROOT/'tests/c_fixture.c'
    asm=compile(fixture,'fixture')
    # The installed passes grow their arenas with sbrk; that build must
    # produce the same intermediate code and assembly.
    sc0,sc1=d/'sc0',d/'sc1'
    subprocess.run(['cc','-std=gnu99','-O1','-w','-DNX_SBRK_ARENA',*[f'user/c/{n}.c' for n in C0],'-o',str(sc0)],cwd=ROOT,check=True)
    subprocess.run(['cc','-std=gnu99','-O1','-w','-DNX_SBRK_ARENA',*[f'user/c/{n}.c' for n in C1],str(table),'-o',str(sc1)],cwd=ROOT,check=True)
    subprocess.run([str(sc0),str(fixture),str(d/'s.1'),str(d/'s.2')],check=True)
    subprocess.run([str(sc1),str(d/'s.1'),str(d/'s.2'),str(d/'s.s')],check=True)
    assert (d/'s.1').read_bytes()==(d/'fixture.1').read_bytes() and (d/'s.s').read_bytes()==asm.read_bytes()
    subprocess.run(['cc','-std=gnu89','-w','-fno-builtin','-no-pie',str(fixture),'-o',str(d/'oracle')],check=True)
    oracle=subprocess.run([str(d/'oracle')],capture_output=True,timeout=20,check=True).stdout
    assert len(oracle.splitlines())==25
    for assembler in ('gnu','native'):
        r=execute(asm,'fixture-'+assembler,assembler)
        assert r.returncode==0 and r.stdout==oracle,(assembler,r.stdout,oracle)
    (OUT/'fixture.c').write_bytes(fixture.read_bytes());(OUT/'fixture.exp').write_bytes(oracle)
    # LP64 address constants: long offsets on either side and unfolded sums.
    addrcon=ROOT/'tests/c_addrcon.c'
    subprocess.run(['cc','-std=gnu89','-w','-fno-builtin','-no-pie',str(addrcon),'-o',str(d/'addrcon-oracle')],check=True)
    want=subprocess.run([str(d/'addrcon-oracle')],capture_output=True,timeout=20,check=True).stdout
    assert want==b'13 12 11 11\n1 11 1 1\n4 ress z\n',want
    asm_ac=compile(addrcon,'addrcon')
    for assembler in ('gnu','native'):
        r=execute(asm_ac,'addrcon-'+assembler,assembler)
        assert r.returncode==0 and r.stdout==want,(assembler,r.stdout)
    (OUT/'addrcon.c').write_bytes(addrcon.read_bytes());(OUT/'addrcon.exp').write_bytes(want)
    # Task 200: the optimized program must behave identically.
    opt=optimize(asm)
    for assembler in ('gnu','native'):
        r=execute(opt,'fixture-opt-'+assembler,assembler)
        assert r.returncode==0 and r.stdout==oracle,(assembler,r.stdout)
    import re
    def split(path,prefix):
        """Bounded translation units keep the native assembler's working
        set within the 16 MiB baseline: shared types and declarations,
        globals defined in the first unit and declared extern elsewhere."""
        lines=path.read_text().split('\n')
        rettype=r'^(int|long|unsigned|unsigned long|double|float|struct \w+|static int|static long)$'
        first=next(i for i,l in enumerate(lines) if re.match(rettype,l) and i+1<len(lines) and re.match(r'^\w+\(',lines[i+1]))
        head=lines[:first]
        shared,defs,externs,intype=[],[],[],False
        for l in head:
            if l.startswith(('struct','union')) and '{' in l and '=' not in l and l.rstrip().endswith('};'):
                shared.append(l)
                continue
            if l.startswith(('struct','union')) and l.rstrip().endswith('{'):
                intype=True
            if intype:
                shared.append(l)
                if l.startswith('};'):
                    intype=False
                continue
            if l.rstrip().endswith(');'):
                shared.append(l)
                continue
            m=re.match(r'^(\w[\w ]*?(?:\s|\*))\s*(\**\w+(?:\[\d*\])?)\s*(=.*)?;$',l.replace('\t',' '))
            if m:
                defs.append(l)
                externs.append('extern '+m.group(1).strip()+' '+m.group(2)+';')
        body=lines[first:]
        starts=[i for i,l in enumerate(body) if (re.match(rettype,l) and i+1<len(body) and re.match(r'^\w+\(',body[i+1])) or l=='main()']
        decls=[body[i]+' '+body[i+1].split('(')[0]+'();' for i in starts
               if body[i] not in ('main()','int') and not body[i].startswith('static') and not body[i+1].split('(')[1].strip(')')[:4] in ('int ','floa','doub','stru')]
        chunks=['\n'.join(body[i:(starts[k+1] if k+1<len(starts) else len(body))]) for k,i in enumerate(starts)]
        groups,size=[],0
        for c in chunks:
            n=c.count('\n')+1
            if not groups or size+n>45:
                groups.append([]);size=0
            groups[-1].append(c);size+=n
        parts=[]
        for n,group in enumerate(groups):
            text='\n'.join(shared)+'\n'+'\n'.join(defs if n==0 else externs)+'\n'+'\n'.join(decls)+'\n'+'\n'.join(group)+'\n'
            name=f'{prefix}{n}';parts.append(name);(OUT/(name+'.c')).write_text(text)
        objs=[]
        for name in parts:
            a2=compile(OUT/(name+'.c'),name)
            assert len(a2.read_text().splitlines())<900,name
            r=run([str(nas),'-o',str(d/(name+'.o')),str(a2)]);assert r.returncode==0,r.stderr
            objs.append(str(d/(name+'.o')))
        (OUT/(prefix+'.txt')).write_text(' '.join(parts)+'\n')
        return objs
    objs=split(fixture,'fpart')
    subprocess.run(['cc','-no-pie',*objs,'-o',str(d/'parts')],check=True)
    r=subprocess.run([str(d/'parts')],capture_output=True,timeout=20)
    assert r.returncode==0 and r.stdout==oracle,r.stdout
    # Task 199: floating values against GCC, and the System V structure,
    # floating and variadic ABI against GCC callers and callees.
    flt=ROOT/'tests/c_float_fixture.c'
    asm=compile(flt,'float')
    subprocess.run(['cc','-std=gnu89','-w','-fno-builtin','-no-pie',str(flt),'-o',str(d/'foracle')],check=True)
    foracle=subprocess.run([str(d/'foracle')],capture_output=True,timeout=20,check=True).stdout
    for assembler in ('gnu','native'):
        r=execute(asm,'float-'+assembler,assembler)
        assert r.returncode==0 and r.stdout==foracle,(assembler,r.stdout,foracle)
    (OUT/'float.exp').write_bytes(foracle)
    r=execute(optimize(asm),'float-opt','native');assert r.returncode==0 and r.stdout==foracle,r.stdout
    objs=split(flt,'flpart')
    subprocess.run(['cc','-no-pie',*objs,'-o',str(d/'flparts')],check=True)
    assert subprocess.run([str(d/'flparts')],capture_output=True,timeout=20).stdout==foracle
    abi=ROOT/'tests/c_abi_fixture.c';driver=ROOT/'tests/c_abi_driver.c'
    asm=compile(abi,'abi')
    PASSABI=b'PASS: native V7 C structure, floating and variadic ABI\n'
    for assembler in ('gnu','native'):
        obj=d/('abi-'+assembler+'.o')
        if assembler=='gnu':
            subprocess.run(['as','--64',str(asm),'-o',str(obj)],check=True)
        else:
            r=run([str(nas),'-o',str(obj),str(asm)]);assert r.returncode==0,r.stderr
        subprocess.run(['cc','-no-pie',str(driver),str(obj),'-o',str(d/'abi')],check=True)
        r=subprocess.run([str(d/'abi')],capture_output=True,timeout=20)
        assert r.returncode==0 and r.stdout==PASSABI,(assembler,r.stdout)
    opt=optimize(asm);obj=d/'abi-opt.o'
    r=run([str(nas),'-o',str(obj),str(opt)]);assert r.returncode==0,r.stderr
    subprocess.run(['cc','-no-pie',str(driver),str(obj),'-o',str(d/'abiopt')],check=True)
    assert subprocess.run([str(d/'abiopt')],capture_output=True,timeout=20).stdout==PASSABI
    objs=split(abi,'apart')
    subprocess.run(['cc','-no-pie',str(driver),*objs,'-o',str(d/'aparts')],check=True)
    assert subprocess.run([str(d/'aparts')],capture_output=True,timeout=20).stdout==PASSABI
    # Native caller for guest linking; the fixture bodies stay c1 output.
    subprocess.run(['gcc','-std=c11','-O2','-ffreestanding','-fno-builtin','-fno-pie','-fno-stack-protector','-fno-asynchronous-unwind-tables',
        '-mno-red-zone','-mno-avx','-nostdinc','-Ilibc/include','-Iinclude','-c',str(driver),'-o',str(OUT/'abidriver.o')],cwd=ROOT,check=True)
    asm=compile(ROOT/'tests/c_v7dialect.c','dialect')
    r=execute(asm,'dialect','native');assert r.stdout==DIALECT.encode(),r.stdout
    (OUT/'dialect.c').write_bytes((ROOT/'tests/c_v7dialect.c').read_bytes());(OUT/'dialect.exp').write_text(DIALECT)

    # Every installed libc header is consumed by the native header profile.
    headers=sorted(str(p.relative_to(ROOT/'libc/include')) for p in (ROOT/'libc/include').rglob('*.h'))
    assert len(headers)>=49
    for h in headers:
        text=subprocess.run(['cc','-E','-P','-nostdinc','-Ilibc/include','-Iinclude','-x','c','-'],input=f'#include <{h}>\nint probe(){{return 1;}}\n'.encode(),cwd=ROOT,capture_output=True,check=True).stdout
        (d/'h.i').write_bytes(text)
        compile(d/'h.i','header')
    # The supplied V7 command sources compile, optimize and assemble natively.
    removed=0
    for name in V7_SOURCES:
        text=subprocess.run(['cc','-E','-P','-nostdinc','-Ilibc/include','-Iinclude','-x','c',f'reference/v7/usr/src/cmd/{name}.c'],cwd=ROOT,capture_output=True,check=True).stdout
        (d/'v7.i').write_bytes(text)
        asm=compile(d/'v7.i','v7-'+name)
        r=run([str(nas),'-o',str(d/'v7.o'),str(asm)]);assert r.returncode==0,(name,r.stderr)
        r=run([str(c2),str(asm),str(d/'v7o.s')]);assert r.returncode==0,(name,r.stderr)
        r=run([str(nas),'-o',str(d/'v7o.o'),str(d/'v7o.s')]);assert r.returncode==0,(name,r.stderr)
        removed+=len(asm.read_text().splitlines())-len((d/'v7o.s').read_text().splitlines())
    assert removed>2000,removed
    for name in ('wc','echo','cat'):
        (OUT/(name+'.c')).write_bytes((ROOT/f'reference/v7/usr/src/cmd/{name}.c').read_bytes())

    # Diagnostics and malformed passes fail without sanitizer reports.
    for i,bad in enumerate(['int f( { return 1; }','main() { x = ; }','int a[-1];','struct s { int a; } v; int f() { return v; }',
                            'main() { goto *; }','int f(int x, ...) { int y; __builtin_va_start(y, x); }',
                            '_Static_assert(sizeof(int) == 8, "lp64 int");','char c = "toolong"[;']):
        (d/'bad.c').write_text(bad+'\n')
        r=run([str(c0),str(d/'bad.c'),str(d/'b1'),str(d/'b2')]);assert r.returncode!=0,bad
    r=run([str(c0)]);assert r.returncode!=0
    r=run([str(c2),str(d/'missing.s')]);assert r.returncode==1 and b"can't find" in r.stderr
    (d/'long.s').write_text('x'*5000+'\n');r=run([str(c2),str(d/'long.s'),str(d/'long.o')]);assert r.returncode==1
    (OUT/'table.s').write_bytes((ROOT/'user/c/table.s').read_bytes());(OUT/'table.exp').write_bytes(table.read_bytes())
    r=run([str(c1)]);assert r.returncode!=0
    (d/'g1').write_bytes(b'\x01\x02\x03');(d/'g2').write_bytes(b'')
    r=run([str(c1),str(d/'g1'),str(d/'g2'),str(d/'g.s')]);assert r.returncode!=0
    good=(d/'fixture.1').read_bytes()
    for cut in (7,len(good)//2,len(good)-3):
        (d/'g1').write_bytes(good[:cut])
        r=run([str(c1),str(d/'g1'),'-',str(d/'g.s')]);assert r.returncode!=0
    # Deterministic source mutations: any status is acceptable, memory errors are not.
    import random
    rng=random.Random(197)
    tokens=['(',')','{','}',';',',','*','&','[',']','=','+','-','"','\'','0x','1e','struct','int','->','.','?',':','<<=','=+']
    for i in range(600):
        text=(fixture if i%2 else abi).read_text()
        t=list(text)
        for _ in range(rng.randint(1,4)):
            k=rng.randrange(len(t))
            op=rng.randrange(3)
            if op==0: del t[k]
            elif op==1: t.insert(k,rng.choice(tokens))
            else: t[k:k+rng.randint(1,20)]=[]
        (d/'m.c').write_text(''.join(t))
        r=run([str(c0),str(d/'m.c'),str(d/'m1'),str(d/'m2')])
        if r.returncode==0:
            run([str(c1),str(d/'m1'),str(d/'m2'),str(d/'m.s')])
    # The optimizer survives malformed jump graphs and segments.
    lines=(d/'fixture.s').read_text().split('\n')
    for i in range(300):
        t=list(lines)
        for _ in range(rng.randint(1,6)):
            k=rng.randrange(len(t));op=rng.randrange(4)
            if op==0: del t[k]
            elif op==1: t.insert(k,rng.choice(t))
            elif op==2: t[k]=t[k].replace('.L','.L1',1)
            else: t.insert(k,rng.choice(['\tjmp\t.L%d'%rng.randrange(1,50),'.L%d:'%rng.randrange(1,50),'\tje\t.L7','\t.text','\t.data','.globl x','\tjmp\t*%rax']))
        (d/'m.s').write_text('\n'.join(t))
        run([str(c2),str(d/'m.s'),str(d/'mo.s')])
print('PASS: V7 C compiler c0/c1/c2/cvopt sanitizers, optimized/unoptimized equivalence, GCC integer/floating execution oracles, structure/floating/variadic ABI with GCC callers and callees through GNU and native as, V7 dialect, libc headers, '
      f'{len(V7_SOURCES)} supplied command sources and malformed-input failures and 600 source and 300 assembly mutations')
