"""Production linker CLI, relocatable relinks, library search and transactional writes."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-ld'
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
with tempfile.TemporaryDirectory(prefix='nx-ld-driver-') as tmp:
    d=Path(tmp);binary=d/'ld'
    flags=['cc','-D_POSIX_C_SOURCE=200809L','-std=c11','-g','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/ld','-Iuser/toolchain']
    subprocess.run(flags+['user/v7/cmd/ld.c','user/ld/resolve.c','user/ld/output.c','user/ld/relocatable.c','user/toolchain/object.c','user/toolchain/archive.c','-o',str(binary)],cwd=ROOT,check=True)
    def run(args,ok=True):
        p=subprocess.run([str(binary),*map(str,args)],cwd=d,env=ENV,capture_output=True,timeout=15)
        assert p.returncode>=0 and b'Sanitizer' not in p.stderr and b'runtime error:' not in p.stderr,p
        assert (p.returncode==0)==ok,p
        return p
    run([OUT/'hoststart.o',OUT/'calculate.o']);subprocess.run([str(d/'a.out')],check=True)
    assert (d/'a.out').read_bytes()==(OUT/'host.elf').read_bytes()
    run(['-niOSt','-o','aliases',OUT/'hoststart.o',OUT/'calculate.o'])
    assert (d/'aliases').read_bytes()==(d/'a.out').read_bytes()
    run(['-r','-o','joined.o',OUT/'hoststart.o',OUT/'calculate.o'])
    run(['-o','fromjoined','joined.o']);subprocess.run([str(d/'fromjoined')],check=True)
    # GNU can consume our -r object, and our linker consumes GNU's -r result.
    subprocess.run(['ld','-Ttext=0x40000000','-o',str(d/'gnulinked'),str(d/'joined.o')],check=True)
    subprocess.run([str(d/'gnulinked')],check=True)
    subprocess.run(['ld','-r','-o',str(d/'gnu.o'),str(OUT/'hoststart.o'),str(OUT/'calculate.o')],check=True)
    run(['-o','fromgnu','gnu.o']);subprocess.run([str(d/'fromgnu')],check=True)
    run(['-r','-u','forced_missing','-o','forced.o',OUT/'local.o'])
    assert ' U forced_missing' in subprocess.check_output(['nm',str(d/'forced.o')]).decode()
    run(['-r','-o','forced-again.o','forced.o'])
    # Repeated -r preserves local and global relocation indices across stages.
    run(['-rx','-o','part1.o',OUT/'hoststart.o'])
    run(['-r','-o','part2.o',OUT/'calculate.o'])
    run(['-r','-o','again.o','part1.o','part2.o'])
    run(['-o','again','again.o']);subprocess.run([str(d/'again')],check=True)
    # Force commons to BSS in a partial link; preserve them by default.
    run(['-rd','-o','allocated.o',OUT/'calculate.o'])
    assert ' B common' in subprocess.check_output(['nm',str(d/'allocated.o')]).decode()
    run(['-r','-o','unallocated.o',OUT/'calculate.o'])
    assert ' C common' in subprocess.check_output(['nm',str(d/'unallocated.o')]).decode()
    run(['-o','allocated',OUT/'hoststart.o','allocated.o']);subprocess.run([str(d/'allocated')],check=True)
    run(['-r','-D','128','-o','padding.o',OUT/'calculate.o'])
    run(['-o','padded',OUT/'hoststart.o','padding.o']);subprocess.run([str(d/'padded')],check=True)
    run(['-s','-o','stripped',OUT/'hoststart.o',OUT/'calculate.o'])
    assert (d/'stripped').stat().st_size<(d/'a.out').stat().st_size
    assert not subprocess.run(['nm',str(d/'stripped')],capture_output=True).stdout
    subprocess.run([str(d/'stripped')],check=True)
    run(['-x','-o','nolocals',OUT/'hoststart.o',OUT/'calculate.o'])
    assert 'zero' not in subprocess.check_output(['nm',str(d/'nolocals')]).decode()
    run(['-u','calculate','-L'+str(OUT),'-e','_start','-o','libraries',OUT/'hoststart.o',OUT/'calculate.o'])
    subprocess.run(['ar','rcs',str(d/'libcalc.a'),str(OUT/'calculate.o')],check=True)
    run(['-o','library',OUT/'hoststart.o','-lcalc','-L',d]);subprocess.run([str(d/'library')],check=True)
    run(['-D','1024','-o','datamin',OUT/'hoststart.o',OUT/'calculate.o']);subprocess.run([str(d/'datamin')],check=True)
    # Native C/libc through the public command, and through an intermediate -r.
    run(['-o','native',ROOT/'build/crt0.o',OUT/'native.o',ROOT/'build/libc.a'])
    assert (d/'native').read_bytes()==(OUT/'native.elf').read_bytes()
    run(['-r','-o','native-part.o',ROOT/'build/crt0.o',OUT/'native.o',ROOT/'build/libc.a'])
    run(['-o','native-again','native-part.o'])
    (OUT/'native-part.o').write_bytes((d/'native-part.o').read_bytes())
    for args in [[],['-o'],['-v','overlay'],['-z'],['-L'],['-lmissing'],['-D','-1'],['-D','18446744073709551616'],
                 ['-o','preserve',OUT/'hoststart.o'],['-o','absent/output',OUT/'hoststart.o',OUT/'calculate.o'],
                 ['-r','-o','preserve',OUT/'broken.o'],['-o','preserve',OUT/'relocs.o',OUT/'wide.o']]:
        (d/'preserve').write_bytes(b'original');run(args,False);assert (d/'preserve').read_bytes()==b'original'
    # Preserve valid DWARF relocation graphs in -r; -S discards debug sections.
    debugsource=d/'debug.c';debugsource.write_text('int debug_data; int debug_function(void){return debug_data;}\n')
    subprocess.run(['cc','-g','-fno-pie','-fno-asynchronous-unwind-tables','-c',str(debugsource),'-o',str(d/'debug.o')],check=True)
    run(['-r','-o','debug-linked.o','debug.o'])
    run(['-rS','-o','debug-stripped.o','debug.o'])
    assert '.debug_info' in subprocess.check_output(['readelf','-S',str(d/'debug-linked.o')]).decode()
    assert '.debug_info' not in subprocess.check_output(['readelf','-S',str(d/'debug-stripped.o')]).decode()
    # Set-ID bits from an existing destination are never inherited.
    (d/'a.out').chmod(0o4755);run([OUT/'hoststart.o',OUT/'calculate.o'])
    assert (d/'a.out').stat().st_mode&0o7000==0
    assert not list(d.glob('.nxa*')) and not list(d.glob('.nxb*'))
print('PASS: ld CLI, -r/-d relinks and GNU interoperability, options, libraries, stripping, native libc and transactional failures')
