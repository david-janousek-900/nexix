"""lint1 over the retained PCC front end and lint2, with the native description libraries."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
FIX=ROOT/'tests/lint'
OUT=ROOT/'build/fixtures-lint';OUT.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
PROFILE='#include "%s"\n'%(ROOT/'rootfs/usr/lib/pcc.h')

def clean(r):
    assert b'Sanitizer' not in r.stderr and b'runtime error:' not in r.stderr,r.stderr
    return r

with tempfile.TemporaryDirectory(prefix='nx-lint-') as tmp:
    d=Path(tmp);lint1=d/'lint1';lint2=d/'lint2'
    flags=['cc','-std=gnu89','-O1','-g','-w','-fcommon','-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/pcc']
    objs=[]
    for name in ('common','trees','match','allo','reader'):
        obj=d/('mip-'+name+'.o');objs.append(str(obj))
        subprocess.run([f.replace('-std=gnu89','-std=c11') for f in flags]+['-c','user/mip/'+name+'.c','-o',str(obj)],cwd=ROOT,check=True)
    for src,extra in (('user/pcc/scan.c',['-DLINT']),('user/pcc/local.c',['-Dclocal=pcc_native_clocal','-Dandable=pcc_native_andable','-Dexname=pcc_native_exname']),('user/lint/lint.c',['-Iuser/lint'])):
        obj=d/(Path(src).stem+'.o');objs.append(str(obj))
        subprocess.run(flags+extra+['-c',src,'-o',str(obj)],cwd=ROOT,check=True)
    front=['pftn','trees','optim','xdefs','common','numbers','ir','declarations','real','abi']
    subprocess.run(flags+[f'user/pcc/{n}.c' for n in front]+['build/gen-pcc/cgram.c',*objs,'-o',str(lint1)],cwd=ROOT,check=True)
    subprocess.run(flags+['-Iuser/lint','user/lint/lpass2.c','-o',str(lint2)],cwd=ROOT,check=True)
    work=d/'w';shutil.copytree(FIX,work)

    def pass1(path,label,opts='',library=False):
        """The installed driver's pipeline for one input: profile, #line, cpp, lint1."""
        text=PROFILE+'#line 1 "%s"\n'%path+(Path(path) if Path(path).is_absolute() else work/path).read_text()
        pre=subprocess.run(['cpp','-C','-Dlint','-nostdinc','-Ilibc/include','-Iinclude','-I'+str(work)],input=text.encode(),cwd=ROOT,capture_output=True,check=True).stdout
        return clean(subprocess.run([str(lint1),('-v' if library else '')+opts+'-L'+label],input=pre,capture_output=True,env=ENV,timeout=20))
    def lint(files,opts='',lib='lc'):
        records=b'';diags=b''
        for f in files:
            r=pass1(f,f,opts);assert r.returncode==0,r.stderr;records+=r.stdout;diags+=r.stderr
        if lib:
            r=pass1(str(ROOT/f'rootfs/usr/lib/llib-{lib}'),'-lc',opts,library=True)
            assert r.returncode==0 and not r.stderr,r.stderr;records+=r.stdout
        r=clean(subprocess.run([str(lint2)]+([opts] if opts else []),input=records,capture_output=True,env=ENV,timeout=20))
        assert r.returncode==0,r.stderr
        return diags.decode(),r.stdout.decode(),records

    # Description libraries agree with the native headers: no diagnostics.
    for lib in ('lc','port','lm'):
        r=pass1(str(ROOT/f'rootfs/usr/lib/llib-{lib}'),'-l'+lib,library=True)
        assert r.returncode==0 and not r.stderr,(lib,r.stderr)
        assert b'\t002\t' in r.stdout and b'\t001\t' not in r.stdout,lib
    r=pass1(str(ROOT/'rootfs/usr/lib/llib-lc'),'-lc',library=True)
    assert b'printf\t002\t104\t-1\t42\t' in r.stdout and b'strlen\t002\t117\t1\t42\t' in r.stdout,r.stdout
    # A prototype-clean native program against llib-lc: silent.
    diags,report,_=lint(['clean.c'])
    assert diags=='' and report=='',(diags,report)
    # Two inconsistent translation units.
    diags,report,records=lint(['main.c','util.c'])
    (OUT/'expected-default').write_text(diags+report)
    expected_diags=['main.c:7: warning: k unused in function main',
        'main.c:6: warning: argument argc unused in function main',
        'util.c:6: warning: argument a unused in function vfun',
        'util.c:6: warning: argument fmt unused in function vfun',
        'util.c:7: warning: argument p unused in function setonly',
        'util.c:7: warning: x set but not used in function setonly',
        'util.c:10: warning: unsigned comparison with 0?',
        'util.c:11: warning: i evaluation order undefined',
        'util.c:12: warning: function mixed has return(e); and return;']
    assert diags.splitlines()==expected_diags,diags
    expected=['helper returns value which is always ignored',
        'noret value is used, but none returned',
        'printf returns value which is always ignored',
        'setonly defined( util.c(7) ), but never used',
        'total value declared inconsistently\tutil.c(3)  ::  main.c(5)',
        'total defined( util.c(3) ), but never used',
        'twice: variable # of args.\tutil.c(1)  ::  main.c(8)',
        'usenoret defined( util.c(14) ), but never used',
        'vfun returns value which is always ignored']
    assert report.splitlines()==expected,report
    # Sorting is lint2's own: sorted, unsorted and duplicated input agree.
    lines=records.splitlines(True)
    for variant in (b''.join(sorted(set(lines))),b''.join(reversed(lines))+records):
        r=clean(subprocess.run([str(lint2)],input=variant,capture_output=True,env=ENV,timeout=20))
        assert r.returncode==0 and r.stdout.decode()==report,r.stdout
    # -h heuristics, -u (no used/defined complaints), -x (unused externs), -p names.
    diags,report,_=lint(['main.c','util.c'],'-hu')
    assert 'null effect' not in diags and 'defined(' not in report and 'used inconsistently' not in report,report
    assert 'twice: variable # of args.' in report
    diags,report,_=lint(['main.c','util.c'],'-x')
    assert 'lonelyext' not in report
    r=pass1('main.c','main.c','-x');assert b'strcpy\t008\t302\t0\t"???"' in r.stdout,r.stdout
    r=pass1('main.c','main.c','-p');assert b'lonely\t128\t' in r.stdout and b'lonelyext' not in r.stdout,r.stdout
    # -v keeps quiet about arguments; ARGSUSED silences one function.
    diags,_,_=lint(['util.c'],'-v');assert 'argument' not in diags,diags
    assert 'argument p unused in function quiet' not in lint(['util.c'])[0]
    # Library definitions: LINTLIBRARY records are LIB and calls are checked against them.
    diags,report,_=lint(['lib.c','uselib.c'],lib=None);(OUT/'expected-lib').write_text(diags+report)
    assert diags.splitlines()==['lib.c:2: warning: argument a unused in function libfn','lib.c:2: warning: argument b unused in function libfn'],diags
    assert report.splitlines()==['libfn, arg. 2 used inconsistently\tlib.c(2)  ::  uselib.c(2)','libfn, arg. 1 used inconsistently\tlib.c(2)  ::  uselib.c(2)','libfn returns value which is always ignored'],report
    # -a precision, -b unreachable break, NOTREACHED.
    plain=pass1('extra.c','extra.c').stderr.decode().splitlines()
    assert plain==['extra.c:3: warning: function dead has return(e); and return;','extra.c:6: warning: statement not reached'],plain
    flagged=pass1('extra.c','extra.c','-ab').stderr.decode().splitlines()
    assert flagged==['extra.c:2: warning: long assignment may lose accuracy',plain[0],'extra.c:5: warning: statement not reached',plain[1]],flagged
    # Malformed pass-2 input fails without memory errors.
    for bad in (b'name',b'f\tx\t4\t0\tf.c\t1\tL\n',b'f\t001\t9\t0\tf.c\t1\tL\n',b'f\t001\t4\t99\t'+b'4\t'*99+b'f.c\t1\tL\n',
                b'f\t001\t4\t1\t4\n',b'f'*5000+b'\t001\t4\t0\tf.c\t1\tL\n',b'\t001\t4\t0\t'+b'x'*300+b'\t1\tL\n'):
        r=clean(subprocess.run([str(lint2)],input=bad,capture_output=True,env=ENV,timeout=20))
        assert r.returncode==1 and r.stderr.startswith(b'pass 2 error: '),(bad[:20],r)
    r=clean(subprocess.run([str(lint2)],input=b'',capture_output=True,env=ENV,timeout=20));assert r.returncode==0 and not r.stdout
    # Malformed C reaches pass 1 errors without crashing.
    for bad in ('int f( { return 1; }\n','main() { x = ; }\n','struct s { int a; } f() { struct s v; return v.b; }\n','#line 3 "q.c"\nint a[-1];\n'):
        (work/'bad.c').write_text(bad)
        r=pass1('bad.c','bad.c');assert b'error' in r.stderr,r.stderr
    for f in FIX.iterdir(): shutil.copy(f,OUT/f.name)
print('PASS: lint1/lint2 records, warnings, cross-file checks, VARARGS/ARGSUSED/LINTLIBRARY, -h/-u/-x/-v/-p, native description libraries, lint2 sorting and malformed input')
