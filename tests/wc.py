"""V7 ASCII word classification and byte/line totals under sanitizers."""
import os
from pathlib import Path
import subprocess
import tempfile
with tempfile.TemporaryDirectory() as directory:
    binary=Path(directory)/'wc';path=Path(directory)/'text'
    subprocess.run(['cc','-std=gnu89','-funsigned-char','-fno-builtin','-fno-pie','-no-pie','-w','-fsanitize=address,undefined','-g','user/v7/cmd/wc.c','-o',str(binary)],check=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
    def check(args,output,status=0,data=None):
        result=subprocess.run([str(binary),*map(str,args)],input=data,capture_output=True,env=env,timeout=5)
        assert (result.returncode,result.stdout)==(status,output.encode()),(args,result,output)
        assert b'Sanitizer' not in result.stderr and b'runtime error' not in result.stderr,result.stderr
    check([],f'{2:7}{3:7} {14:7}\n',data=b'one two\nthree\n')
    check(['-cwl'],f'{14:7}{3:7} {2:7}\n',data=b'one two\nthree\n')
    check([],f'{0:7}{0:7} {0:7}\n',data=b'')
    check([],f'{1:7}{2:7} {8:7}\n',data=b'a\x00b\v\xff c\n')
    path.write_bytes(b'x\n'*10000)
    check(['-l',path,path],f'{10000:7} {path}\n'*2+f'{20000:7} total\n')
    check(['-l',str(path)+'missing',path],f'{10000:7} {path}\n{10000:7} total\n',1)
    check(['-z'],'',1)
print('PASS: wc ASan/UBSan V7 word classification, bytes, lines, option order, totals and errors')
