"""Source-defined cmp bytes, positions, skips and errors under sanitizers."""
import os
from pathlib import Path
import subprocess
import tempfile
with tempfile.TemporaryDirectory() as directory:
    binary=Path(directory)/'cmp';a=Path(directory)/'a';b=Path(directory)/'b'
    subprocess.run(['cc','-std=gnu89','-funsigned-char','-fno-builtin','-w','-fsanitize=address,undefined','-fno-pie','-no-pie','-g','user/v7/cmd/cmp.c','-o',str(binary)],check=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
    def check(args,out='',status=0,data=None):
        result=subprocess.run([str(binary),*map(str,args)],input=data,capture_output=True,env=env,timeout=5)
        assert (result.returncode,result.stdout)==(status,out.encode()),(args,result)
        assert b'Sanitizer' not in result.stderr and b'runtime error' not in result.stderr,result.stderr
    a.write_bytes(b'alpha\nbeta\n');b.write_bytes(a.read_bytes());check([a,b])
    check(['-',b],data=a.read_bytes())
    b.write_bytes(b'alpha\nBeta\n');check([a,b],f'{a} {b} differ: char 7, line 2\n',1)
    check(['-l',a,b],'     7 142 102\n',1);check(['-s',a,b],status=1)
    b.write_bytes(b'alpha\n');check([a,b],f'cmp: EOF on {b}\n',1);check(['-s',a,b],status=1)
    b.write_bytes(b'Xalpha\nbeta\n');check([a,b,'0','1'])
    b.write_bytes(b'0'*8+a.read_bytes());check([a,b,'0','010'])
    check([a,b,'999999999999999999999'], 'cmp: invalid skip\n',2)
    check([a,b,'08'],'cmp: invalid skip\n',2)
    check(['-s',a,str(b)+'missing'],status=2)
    a.write_bytes(bytes(range(256))*20);b.write_bytes(a.read_bytes());check([a,b])
print('PASS: cmp ASan/UBSan binary bytes, line positions, lists, stdin, octal/decimal skips and statuses')
