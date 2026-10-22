"""PCC aggregate, long-double and variadic System V ABI against GCC callers/callees."""
from pathlib import Path
import json
import os
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-pcc-abi';OUT.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
PROFILE=ROOT/'user/pcc/native.h'
source=(ROOT/'tests/pcc_abi_fixture.c').read_text().splitlines()
types=[l for l in source if l.startswith(('struct ','union ')) and l.endswith('};') and '(' not in l]
functions=[l for l in source if '{' in l and l not in types]
assert len(functions)>=50
globals_=[l for l in source if l and l not in types and l not in functions and not l.startswith(('/*',' *'))]
# Separately compiled units see every fixture function through its prototype.
prototypes=[f[:f.index('{')]+';' for f in functions if not f.startswith('long kr_pair(')]
prototypes.append('long kr_pair();')
HEADER='#include "/tmp/native.h"\n#include <stdarg.h>\n'
def module(body,first):
    data=[l if first or not l.startswith('long double ld_global') else 'extern long double ld_global;' for l in globals_]
    return HEADER+'\n'.join(types+data+prototypes+body)+'\n'
def preprocess(text):
    p=subprocess.run(['cc','-E','-P','-nostdinc','-Ilibc/include','-Iinclude','-x','c','-'],input=text.replace('/tmp/native.h',str(PROFILE)).encode(),cwd=ROOT,capture_output=True,check=True)
    return p.stdout
(OUT/'native.h').write_bytes(PROFILE.read_bytes())
whole=module(functions,True);(OUT/'abi.c').write_text(whole)
modules=[]
for start in range(0,len(functions),6):
    name=f'part{start//6}';modules.append(name)
    (OUT/(name+'.c')).write_text(module(functions[start:start+6],start==0))
(OUT/'modules.json').write_text(json.dumps(modules))
with tempfile.TemporaryDirectory(prefix='nx-pcc-abi-') as tmp:
    d=Path(tmp);binary=d/'ccom'
    flags=['cc','-std=gnu89','-O1','-g','-w','-fcommon','-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/pcc']
    mip=[]
    for name in ('common','trees','match','allo','reader'):
        obj=d/('mip-'+name+'.o');mip.append(str(obj))
        subprocess.run([f.replace('-std=gnu89','-std=c11') for f in flags]+['-c','user/mip/'+name+'.c','-o',str(obj)],cwd=ROOT,check=True)
    files=['scan','pftn','trees','optim','xdefs','common','local','numbers','ir','declarations','real','abi','code','local2','output','ccom']
    subprocess.run(flags+[f'user/pcc/{n}.c' for n in files]+['build/gen-pcc/cgram.c',*mip,'-o',str(binary)],cwd=ROOT,check=True)
    def compile(name):
        text=preprocess((OUT/(name+'.c')).read_text());(d/(name+'.i')).write_bytes(text)
        p=subprocess.run([str(binary),'--native-source',str(d/(name+'.i'))],capture_output=True,env=ENV,timeout=20)
        assert p.returncode==0 and not p.stderr,p.stderr
        encoded=subprocess.run([str(binary),'--emit-pass','--native-source',str(d/(name+'.i'))],capture_output=True,env=ENV,timeout=20)
        assert encoded.returncode==0 and not encoded.stderr,encoded.stderr
        decoded=subprocess.run([str(binary),'--read-pass'],input=encoded.stdout,capture_output=True,env=ENV,timeout=20)
        assert decoded.returncode==0 and decoded.stdout==p.stdout and not decoded.stderr,decoded.stderr
        (OUT/(name+'.s')).write_bytes(p.stdout)
        subprocess.run(['as','--64',str(OUT/(name+'.s')),'-o',str(d/(name+'.o'))],check=True)
        return d/(name+'.o')
    driver=ROOT/'tests/pcc_abi_driver.c'
    whole_object=compile('abi')
    objects=[str(compile(name)) for name in modules]
    for label,objs in (('whole',[str(whole_object)]),('modules',objects)):
        subprocess.run(['cc','-no-pie','-O2',str(driver),*objs,'-o',str(d/label)],check=True)
        r=subprocess.run([str(d/label)],capture_output=True,timeout=20)
        assert r.returncode==0 and r.stdout==b'PASS: PCC aggregate/long-double/variadic ABI\n',(label,r)
    # GCC compiling the same fixture is the independent behavior oracle.
    oracle=whole.replace('#include "/tmp/native.h"\n','')
    (d/'oracle.c').write_text(oracle)
    subprocess.run(['cc','-std=gnu99','-no-pie','-O2','-w',str(driver),str(d/'oracle.c'),'-o',str(d/'oracle')],check=True)
    r=subprocess.run([str(d/'oracle')],capture_output=True,timeout=20)
    assert r.returncode==0 and r.stdout==b'PASS: PCC aggregate/long-double/variadic ABI\n',r
    # Native caller for guest linking; the generated bodies remain PCC output.
    subprocess.run(['gcc','-std=c11','-O2','-ffreestanding','-fno-builtin','-fno-pie','-fno-stack-protector','-fno-asynchronous-unwind-tables','-mno-red-zone','-mno-avx','-nostdinc','-Ilibc/include','-Iinclude','-c',str(driver),'-o',str(OUT/'driver.o')],cwd=ROOT,check=True)
    # Invalid aggregate/variadic uses still fail without sanitizer reports.
    for bad in ['struct s {int a;};int f(struct s x){return x;}',
                'long f(int n){va_list ap;va_start(ap,n);return 0;}',
                'struct s {int a;};long f(int n,...){va_list ap;va_start(ap,n);return va_arg(ap,void);}',
                'struct s {int a;};struct t {int b;};int g(struct s);int f(struct t v){return g(v);}']:
        text=preprocess(HEADER+bad+'\n');(d/'bad.i').write_bytes(text)
        r=subprocess.run([str(binary),'--native-source',str(d/'bad.i')],capture_output=True,env=ENV,timeout=20)
        assert r.returncode!=0 and b'Sanitizer' not in r.stderr and b'runtime error:' not in r.stderr,(bad,r)
print('PASS: PCC aggregate classification, structure calls/results, x87 long double and variadic definitions against GCC callers, callees and oracle')
