"""Sanitize native tail's byte, line, ring and reverse paths."""
import os
from pathlib import Path
import subprocess
import tempfile

with tempfile.TemporaryDirectory() as directory:
    binary=Path(directory)/'tail';file=Path(directory)/'input'
    subprocess.run(['cc','-include','user/v7/compat.h','-std=gnu89','-funsigned-char','-fno-builtin','-fno-pie','-no-pie','-w','-fsanitize=address,undefined','-g','user/v7/cmd/tail.c','-o',str(binary)],check=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
    def check(data,args,want,status=0):
        file.write_bytes(data)
        for pipe in (False,True):
            result=subprocess.run([str(binary),*args,*([] if pipe else [str(file)])],input=data if pipe else None,capture_output=True,env=env,timeout=5)
            assert (result.returncode,result.stdout)==(status,want),(args,pipe,result)
            assert b'Sanitizer' not in result.stderr and b'runtime error' not in result.stderr,result.stderr
    lines=b''.join(f'{i:04}\n'.encode() for i in range(2000))
    check(lines,[],lines[-50:]);check(lines,['-3'],lines[-15:])
    check(lines,['+1999l'],lines[-10:]);check(lines,['+1c'],lines)
    check(lines,['+2b'],lines[512:]);check(lines,['-1b'],lines[-512:])
    check(lines,['-20c'],lines[-20:]);check(lines,['-0'],b'')
    check(b'a\nb\nc\n',['-2r'],b'c\nb\n')
    check(b'a\nb\nc',['-r'],b'c\nb\na\n')
    check(b'', ['-r'],b'');check(b'abc',['+1l'],b'abc')
    check(b'abc',['-1'],b'abc');check(b'abc',['+10c'],b'')
    for n in [4096,4097,4098,8194,8195]:
        data=b'x'*n;check(data,['-17c'],data[-17:])
    for arg in ['-999999999999999999999999c','-9999999999999999999b','-2x','-2lc']:
        check(b'x',[arg],b'',1)
print('PASS: tail ASan/UBSan files/pipes, default, +lines/bytes/blocks, suffix/reverse and ring boundaries')
