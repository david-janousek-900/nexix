"""Exercise the public driver through its real sanitizer-instrumented worker."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-as'; OUT.mkdir(exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
# The object producer suite supplies an independent linker/execution oracle.
subprocess.run(['python3','tests/as_objects.py'],cwd=ROOT,check=True)
source=(OUT/'object.s').read_text()
cut=source.index('.data\n')
(OUT/'part1.s').write_text(source[:cut]);(OUT/'part2.s').write_text(source[cut:])
with tempfile.TemporaryDirectory(prefix='nx-as-driver-') as tmp:
    d=Path(tmp);worker=d/'as2';front=d/'as'
    flags=['cc','-D_POSIX_C_SOURCE=200809L','-std=c11','-g','-O1','-fsanitize=address,undefined','-fno-pie','-no-pie','-Wall','-Wextra','-Werror','-Iuser/as','-Iuser/toolchain']
    subprocess.run(flags+['-DAS_WORKER','user/v7/cmd/as.c','user/as/parse.c','user/as/encode.c','user/as/object.c','user/toolchain/archive.c','user/toolchain/object.c','-o',str(worker)],cwd=ROOT,check=True)
    subprocess.run(flags+['-DNX_AS2_PATH="'+str(worker)+'"','user/v7/cmd/as.c','-o',str(front)],cwd=ROOT,check=True)
    def run(args=(),data=None,ok=True):
        r=subprocess.run([str(front),*map(str,args)],input=data,cwd=d,env=ENV,capture_output=True,timeout=15)
        assert r.returncode>=0 and b'Sanitizer' not in r.stderr and b'runtime error:' not in r.stderr,r
        assert (r.returncode==0)==ok,r
        return r
    expected=(OUT/'object.o').read_bytes()
    run([OUT/'object.s']);assert (d/'a.out').read_bytes()==expected
    for options in [[],['-u'],['-'],['--64']]:
        run([*options,'-o','result.o',OUT/'part1.s',OUT/'part2.s'])
        assert (d/'result.o').read_bytes()==expected
    run(['-o','result.o'],source.encode());assert (d/'result.o').read_bytes()==expected
    run(['-o','result.o','--','-'],source.encode());assert (d/'result.o').read_bytes()==expected
    for args in [['-o'],['-bad'],['-o','a','-o','b'],['missing.s'],['-o','absent/out.o',OUT/'object.s']]:run(args,ok=False)
    run(['-o','result.o'],b'.bss\n.byte 1\n',ok=False);assert (d/'result.o').read_bytes()==expected
    (d/'bad.s').write_text('\n.invalid\n')
    r=run(['bad.s'],ok=False);assert b'bad.s:2:' in r.stderr,r.stderr
    # Numeric local labels and sections persist across input boundaries.
    (d/'one.s').write_text('.text\n1: nop\n')
    (d/'two.s').write_text('jmp 1b\n')
    run(['-o','split.o','one.s','two.s'])
    run(['-o','whole.o'],b'.text\n1: nop\njmp 1b\n')
    assert (d/'split.o').read_bytes()==(d/'whole.o').read_bytes()
    worker.unlink();assert b'cannot execute' in run([],ok=False).stderr
print('PASS: public as/as2 pipeline, multi-file state, stdin, options, diagnostics and output preservation')
