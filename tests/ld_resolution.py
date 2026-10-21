"""ELF linker resolution: V7 archive search, GNU ELF oracle and malformed data."""
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
with tempfile.TemporaryDirectory(prefix='nx-ld-resolve-') as tmp:
    binary=Path(tmp)/'resolve'
    run('cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',
        '-Iuser/ld','-Iuser/toolchain','user/ld/resolve.c','user/toolchain/object.c','tests/ld-resolve.c','-o',str(binary),cwd=ROOT)
    def asm(name,text):
        (OUT/(name+'.s')).write_text(text)
        run('as','--64',str(OUT/(name+'.s')),'-o',str(OUT/(name+'.o')))
    asm('main','.globl entry\n.text\nentry: call a\n.quad missing_weak\n.weak missing_weak\n.comm common,4,4\n')
    asm('a','.globl a\n.text\na: call b\n.comm common,16,16\n')
    asm('b','.globl b\n.text\nb: ret\n')
    asm('unused','.globl unused\n.text\nunused: call never_needed\n')
    asm('weak','.weak a\n.text\na: ret\n')
    asm('strong','.globl a\n.text\na: ret\n')
    asm('common','.comm a,32,32\n')
    asm('local','.text\nlocal_only: ret\n')
    asm('absolute','.globl absolute\n.set absolute,0x123456789abcdef0\n')
    asm('long','.globl identifier_beyond_eight_characters\n.text\nidentifier_beyond_eight_characters: ret\n')
    archive=OUT/'libchain.a';archive.unlink(missing_ok=True)
    run('ar','rcs',str(archive),str(OUT/'b.o'),str(OUT/'unused.o'),str(OUT/'a.o'))
    def check(args,ok=True,contains=()):
        args=[str(OUT/a) if a.endswith(('.o','.a','.bad')) else a for a in args]
        p=subprocess.run([str(binary),*args],capture_output=True,env=ENV,timeout=5)
        assert p.returncode==(0 if ok else 1),(args,p)
        assert b'AddressSanitizer' not in p.stderr and b'runtime error:' not in p.stderr,p
        for c in contains:assert c.encode() in p.stdout+p.stderr,(c,p)
        return p
    p=check(['main.o','libchain.a'],contains=('input libchain.a(b.o)','input libchain.a(a.o)',
        'symbol common common global 16 16','symbol missing_weak weak-undefined weak'))
    assert b'input libchain.a(unused.o)' not in p.stdout
    (OUT/'expected').write_bytes(p.stdout)
    check(['libchain.a','main.o'],False,('undefined symbol: a',))
    check(['main.o'],False,('undefined symbol: a',))
    check(['-u','a','libchain.a'],contains=('symbol a defined','symbol b defined'))
    check(['-u','missing'],False,('undefined symbol: missing',))
    check(['strong.o','strong.o'],False,('multiple definition: a',))
    check(['weak.o','strong.o'],contains=('symbol a defined global',))
    check(['strong.o','weak.o'],contains=('symbol a defined global',))
    check(['weak.o','weak.o'],contains=('symbol a defined weak',))
    check(['common.o','strong.o'],contains=('symbol a defined global',))
    check(['strong.o','common.o'],contains=('symbol a defined global',))
    check(['weak.o','common.o'],contains=('symbol a common global 32 32',))
    check(['common.o','weak.o'],contains=('symbol a common global 32 32',))
    check(['local.o','local.o','absolute.o','long.o'],contains=('symbol absolute defined', 'symbol identifier_beyond_eight_characters defined'))
    # GNU linker independently proves the same inclusion and common-size outcomes.
    oracle=Path(tmp)/'oracle.o'
    run('ld','-r','-o',str(oracle),str(OUT/'main.o'),str(archive))
    nm=subprocess.check_output(['nm','-S',str(oracle)]).decode()
    assert ' a\n' in nm and ' b\n' in nm and 'unused' not in nm and 'never_needed' not in nm,nm
    assert '0000000000000010 0000000000000010 C common' in nm,nm
    # Both indexed and unindexed archives have the same fixed-point search.
    run('ar','rcS',str(OUT/'plain.a'),str(OUT/'b.o'),str(OUT/'unused.o'),str(OUT/'a.o'))
    check(['main.o','plain.a'],contains=('input plain.a(b.o)',))
    # Common symbols can cause a real strong definition to be extracted.
    run('ar','rcs',str(OUT/'strong.a'),str(OUT/'strong.o'))
    check(['common.o','strong.a'],contains=('symbol a defined global',))
    # A weak undefined alone never extracts a member.
    asm('weakref','.weak a\n.data\n.quad a\n')
    p=check(['weakref.o','strong.a']);assert b'input strong.a' not in p.stdout
    # Corrupt symbol/string/section metadata and truncated ELF must all fail safely.
    data=(OUT/'main.o').read_bytes();shoff=struct.unpack_from('<Q',data,40)[0]
    shnum=struct.unpack_from('<H',data,60)[0]
    sym=next(shoff+i*64 for i in range(shnum) if struct.unpack_from('<I',data,shoff+i*64+4)[0]==2)
    tab=struct.unpack_from('<Q',data,sym+24)[0]
    bad=[]
    for n in (0,1,63,len(data)-1):bad.append(data[:n])
    for off,fmt,value in [(16,'H',2),(sym+56,'Q',1),(sym+40,'I',65535),(tab+24,'I',0xffffffff),
                          (tab+24+6,'H',65500),(tab+24+4,'B',0xf0),(tab,'I',1)]:
        b=bytearray(data);struct.pack_into('<'+fmt,b,off,value);bad.append(b)
    for b in bad:
        (OUT/'invalid.bad').write_bytes(b);check(['invalid.bad'],False)
    rng=random.Random(185)
    for _ in range(100):
        b=bytearray(data)
        for _ in range(rng.randrange(1,5)):b[rng.randrange(len(b))]=rng.randrange(256)
        (OUT/'fuzz.bad').write_bytes(b)
        p=subprocess.run([str(binary),str(OUT/'fuzz.bad')],capture_output=True,env=ENV,timeout=5)
        assert p.returncode in (0,1) and b'AddressSanitizer' not in p.stderr and b'runtime error:' not in p.stderr,p
    (OUT/'invalid.bad').write_bytes(b'not an object')
print('PASS: ld resolution sanitizers, GNU oracle, archive fixed point/order, weak/common/strong symbols and malformed ELF')
