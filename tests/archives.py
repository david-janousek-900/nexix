"""Independent archive bytes, host interoperability and production sanitizers."""
from pathlib import Path
import os
import random
import struct
import subprocess
import sys
import tempfile
from object_tools import fixtures
ROOT=Path(__file__).resolve().parents[1]
TOOL=sys.argv[1]
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
with tempfile.TemporaryDirectory(prefix='nx-archives-') as temp:
    folder=Path(temp);binary=folder/('tool-'+TOOL)
    subprocess.run(['cc','-D_POSIX_C_SOURCE=200809L','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/toolchain',
        'user/v7/cmd/'+TOOL+'.c','user/toolchain/archive.c','user/toolchain/object.c','-o',str(binary)],cwd=ROOT,check=True)
    obj,bads=fixtures(folder)
    def run(*args,ok=True):
        r=subprocess.run([str(binary),*map(str,args)],cwd=folder,env=ENV,capture_output=True,timeout=10)
        assert r.returncode>=0 and b'AddressSanitizer' not in r.stderr and b'runtime error:' not in r.stderr,r
        if ok:assert r.returncode==0,r
        return r
    for name,data in [('a',b'A\x00odd'),('b',b'B\n'),('empty',b''),('name_with_a_long_component',b'long')]:
        (folder/name).write_bytes(data)
    if TOOL=='ar':
        run('rcs','ours.a','a','b','empty','name_with_a_long_component',obj.name)
        names='a\nb\nempty\nname_with_a_long_component\n'+obj.name+'\n'
        assert run('t','ours.a').stdout.decode()==names
        assert subprocess.check_output(['ar','t',str(folder/'ours.a')]).decode()==names
        assert run('p','ours.a','a','b').stdout==b'A\x00oddB\n'
        # Validate real archive symbol offsets rather than trusting listing alone.
        data=(folder/'ours.a').read_bytes();n=int(data[8+48:8+58]);index=data[68:68+n]
        count=struct.unpack_from('>I',index)[0];assert count>=5
        for i in range(count):
            offset=struct.unpack_from('>I',index,4+4*i)[0]
            assert data[offset+58:offset+60]==b'`\n'
        (folder/'main.c').write_text('int unresolved_external=13; extern int function_with_a_very_long_symbol_name(void); int main(void){return function_with_a_very_long_symbol_name()!=13;}\n')
        subprocess.run(['cc','-no-pie',str(folder/'main.c'),str(folder/'ours.a'),'-o',str(folder/'linked')],check=True)
        subprocess.run([str(folder/'linked')],check=True)
        assert run('tv','ours.a').stdout.count(b'\n')==5
        run('mb','a','ours.a','b');assert run('t','ours.a').stdout.splitlines()[:2]==[b'b',b'a']
        run('ma','empty','ours.a','a');assert run('t','ours.a').stdout.splitlines()[:3]==[b'b',b'empty',b'a']
        (folder/'a').write_bytes(b'replaced');run('r','ours.a','a');assert run('p','ours.a','a').stdout==b'replaced'
        os.utime(folder/'a',(1,1));(folder/'a').write_bytes(b'older');os.utime(folder/'a',(1,1))
        run('ru','ours.a','a');assert run('p','ours.a','a').stdout==b'replaced'
        run('q','ours.a','b');assert run('t','ours.a').stdout.splitlines().count(b'b')==2
        run('d','ours.a','empty');assert b'empty' not in run('t','ours.a').stdout.splitlines()
        (folder/'a').unlink();run('x','ours.a','a');assert (folder/'a').read_bytes()==b'replaced'
        for archive in ('native.a','bsd.a','historical.a'):
            (folder/('read-'+archive)).write_bytes((folder/archive).read_bytes())
            assert run('t',archive).stdout.strip() in (obj.name.encode(),b'fixture.o')
            assert run('p',archive).stdout==obj.read_bytes()
            run('s',archive)
            assert (folder/archive).read_bytes().startswith(b'!<arch>\n')
            subprocess.run(['ar','t',str(folder/archive)],check=True,stdout=subprocess.DEVNULL)
        saved=(folder/'ours.a').read_bytes()
        for args in [('r','ours.a','absent'),('d','ours.a','absent'),('mb','absent','ours.a','a')]:
            assert run(*args,ok=False).returncode!=0
            assert (folder/'ours.a').read_bytes()==saved
        for args in [('z','ours.a'),('rq','ours.a'),('qa','a','ours.a'),('ruvSsx','ours.a')]:assert run(*args,ok=False).returncode!=0
        # Old V6 container: 8-byte name, PDP-endian date, uid/mode bytes, 16-bit size.
        old=struct.pack('<H',0o177555)+b'old.o\0\0\0'+struct.pack('<HHBBH',0x1234,0x5678,7,0o644&255,3)+b'xyz\0'
        (folder/'v6data').write_bytes(b'xyz')
        (folder/'v6.a').write_bytes(old);assert run('p','v6.a').stdout==b'xyz'
        # Bad header lengths, embedded NULs, escaping member names and truncation.
        hdr=b'../escape/'.ljust(16,b' ')+b'0'.ljust(12)+b'0'.ljust(6)*2+b'644'.ljust(8)+b'1'.ljust(10)+b'`\n'
        malformed=[b'!<arch>\n'+hdr+b'x\n',b'!<arch>\n'+hdr[:59],saved[:-1],b'not an archive',old[:-1]]
        rng=random.Random(179)
        malformed += [bytes(rng.randrange(256) for _ in range(rng.randrange(1,120))) for _ in range(100)]
        for i,data in enumerate(malformed):
            p=folder/'bad.a';p.write_bytes(data)
            assert run('t','bad.a',ok=False).returncode!=0
            assert run('r','bad.a','a',ok=False).returncode!=0
            assert p.read_bytes()==data
        (folder/'guest.a').write_bytes(saved)
    if TOOL=='arcv':
        old=struct.pack('<H',0o177555)+b'old.o\0\0\0'+struct.pack('<HHBBH',0x1234,0x5678,7,0o644&255,3)+b'xyz\0'
        (folder/'v6.a').write_bytes(old)
        for name in ('v6.a','historical.a'):
            before=(folder/name).read_bytes();(folder/('read-'+name)).write_bytes(before)
            run(name);after=(folder/name).read_bytes()
            assert after.startswith(b'!<arch>\n')
            expected=b'xyz' if name=='v6.a' else obj.read_bytes()
            assert subprocess.check_output(['ar','p',str(folder/name)])==expected
            assert int(after[8+16:8+28])==(0x12345678 if name=='v6.a' else 0)
            assert int(after[8+28:8+34])==(7 if name=='v6.a' else 0)
            assert int(after[8+34:8+40])==(1 if name=='v6.a' else 0)
            assert run(name,ok=False).returncode!=0
            assert (folder/name).read_bytes()==after
        assert run(ok=False).returncode==2
        for data in [old[:-1],old[:17],b'not archive',struct.pack('<H',0o177545)+b'bad']:
            (folder/'bad.a').write_bytes(data)
            assert run('bad.a',ok=False).returncode!=0
            assert (folder/'bad.a').read_bytes()==data
        # A failure on one input does not prevent later inputs from converting.
        (folder/'v6.a').write_bytes(old)
        assert run('absent.a','v6.a',ok=False).returncode==1
        assert (folder/'v6.a').read_bytes().startswith(b'!<arch>\n')
    if TOOL=='ranlib':
        # Generate the archive independently, omitting its index deliberately.
        (folder/'index.c').write_text('int common; int strong=4; static int local=3; int weak(void) __attribute__((weak)); int weak(void){return local;} extern int undef_weak(void) __attribute__((weak)); int call(void){return undef_weak?undef_weak():strong;}\n')
        subprocess.run(['cc','-fcommon','-fno-pie','-c',str(folder/'index.c'),'-o',str(folder/'index.o')],check=True)
        subprocess.run(['ar','rcS',str(folder/'unindexed.a'),str(obj),str(folder/'index.o')],check=True)
        (folder/'read-native.a').write_bytes((folder/'unindexed.a').read_bytes())
        run('unindexed.a');data=(folder/'unindexed.a').read_bytes()
        n=int(data[8+48:8+58]);index=data[68:68+n];count=struct.unpack_from('>I',index)[0]
        names=index[4+count*4:].split(b'\0')[:-1]
        assert all(x in names for x in [b'common',b'strong',b'weak',b'call',b'function_with_a_very_long_symbol_name'])
        assert all(x not in names for x in [b'local',b'undef_weak',b'unresolved_external'])
        for i in range(count):
            offset=struct.unpack_from('>I',index,4+4*i)[0]
            assert data[offset+58:offset+60]==b'`\n'
        (folder/'main.c').write_text('int unresolved_external=13; int undef_weak(void){return 4;} extern int function_with_a_very_long_symbol_name(void),call(void); int main(void){return function_with_a_very_long_symbol_name()!=13 || call()!=4;}\n')
        subprocess.run(['cc','-no-pie',str(folder/'main.c'),str(folder/'unindexed.a'),'-o',str(folder/'linked')],check=True)
        subprocess.run([str(folder/'linked')],check=True)
        run('unindexed.a');assert (folder/'unindexed.a').read_bytes()==data
        (folder/'native.a').write_bytes(data)
        for name in ('bsd.a','historical.a'):
            (folder/('read-'+name)).write_bytes((folder/name).read_bytes())
            run(name);assert subprocess.check_output(['ar','p',str(folder/name)])==obj.read_bytes()
        assert run(ok=False).returncode==2
        for data in [b'bad',b'!<arch>\ntruncated']:
            (folder/'bad.a').write_bytes(data)
            assert run('bad.a',ok=False).returncode!=0
            assert (folder/'bad.a').read_bytes()==data
        # A corrupt ELF symbol table must not replace a valid archive container.
        subprocess.run(['ar','rcS',str(folder/'broken.a'),str(folder/'string-offset')],check=True)
        saved=(folder/'broken.a').read_bytes()
        assert run('broken.a',ok=False).returncode!=0
        assert (folder/'broken.a').read_bytes()==saved
    if TOOL=='strip':
        (folder/'app.c').write_text('static const char text[]="strip-execution-ok"; int main(void){return text[0]!=115;}\n')
        subprocess.run(['cc','-g','-no-pie',str(folder/'app.c'),'-o',str(folder/'app')],check=True)
        before=(folder/'app').read_bytes();mode=(folder/'app').stat().st_mode&0o777
        run('app');after=(folder/'app').read_bytes();assert len(after)<len(before)
        assert (folder/'app').stat().st_mode&0o777==mode
        subprocess.run([str(folder/'app')],check=True)
        output=subprocess.check_output(['readelf','-S',str(folder/'app')]);assert b'.symtab' not in output and b'.debug' not in output
        # Every executable program segment retains identical load bytes/addresses.
        phoff=struct.unpack_from('<Q',before,32)[0];phnum=struct.unpack_from('<H',before,56)[0]
        for i in range(phnum):
            p=struct.unpack_from('<IIQQQQQQ',before,phoff+56*i)
            if p[0]==1:
                start=max(p[2],64);assert before[start:p[2]+p[5]]==after[start:p[2]+p[5]]
        run('app');assert (folder/'app').read_bytes()==after
        subprocess.run(['cc','-g',str(folder/'app.c'),'-o',str(folder/'pie')],check=True)
        run('pie');subprocess.run([str(folder/'pie')],check=True)
        # Native ET_REL stripping matches the V7 loss of static relocation info.
        original=obj.read_bytes();run(obj.name)
        output=subprocess.check_output(['readelf','-S',str(obj)]);assert b'.symtab' not in output and b'.rela' not in output
        for archive in ('native.a','bsd.a','historical.a'):
            (folder/('read-'+archive)).write_bytes((folder/archive).read_bytes())
            run(archive)
            assert subprocess.check_output(['ar','p',str(folder/archive)])==obj.read_bytes()
        for name in bads:
            p=folder/name;saved=p.read_bytes();assert run(name,ok=False).returncode!=0;assert p.read_bytes()==saved
        subprocess.run(['ar','rcS',str(folder/'broken.a'),str(folder/'string-offset')],check=True)
        saved=(folder/'broken.a').read_bytes();assert run('broken.a',ok=False).returncode!=0
        assert (folder/'broken.a').read_bytes()==saved
        assert run(ok=False).returncode==2
        rng=random.Random(190)
        for _ in range(100):
            data=bytearray(original)
            for _ in range(3):data[rng.randrange(len(data))]=rng.randrange(256)
            (folder/'mutated.o').write_bytes(data);r=run('mutated.o',ok=False)
            if r.returncode:assert (folder/'mutated.o').read_bytes()==data
        # Exact ELF fixture compiled for the guest loader; run there after stripping.
        subprocess.run(['make','build/v7-echo.elf'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
        (folder/'native.elf').write_bytes((ROOT/'build/v7-echo.elf').read_bytes())
        (folder/'read-native.elf').write_bytes((folder/'native.elf').read_bytes())
        run('native.elf')
    out=ROOT/'build'/('fixtures-'+TOOL);out.mkdir(exist_ok=True)
    for p in folder.iterdir():
        if p.is_file() and p!=binary:(out/p.name).write_bytes(p.read_bytes())
print('PASS: '+TOOL+' production ASan/UBSan, independently checked archives, native format and failure preservation')
