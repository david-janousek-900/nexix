"""Shared host harness: build one retained V7 command with ASan/UBSan and run fixtures."""
import os
from pathlib import Path
import subprocess
import tempfile

class Command:
    def __init__(self, name, sources=None, flags=()):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.binary = self.dir/name
        sources = sources or [f'user/v7/cmd/{name}.c']
        subprocess.run(['cc','-include',str(Path('user/v7/compat.h').resolve()),'-std=gnu89','-funsigned-char','-fcommon','-fno-builtin','-fno-pie','-no-pie','-w',
                        '-fsanitize=address,undefined','-g',*flags,*map(str,sources),'-o',str(self.binary)],check=True)
        self.env = dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
    def run(self, args, data=b'', cwd=None):
        if isinstance(data,str): data = data.encode()
        result = subprocess.run([str(self.binary),*(a if isinstance(a,bytes) else str(a) for a in args)],input=data,capture_output=True,env=self.env,timeout=20,cwd=cwd or self.dir)
        assert b'Sanitizer' not in result.stderr and b'runtime error' not in result.stderr,result.stderr.decode(errors='replace')
        return result
    def check(self, args, stdout=b'', status=0, data=b'', stderr=None, cwd=None):
        if isinstance(stdout,str): stdout = stdout.encode()
        if isinstance(data,str): data = data.encode()
        if isinstance(stderr,str): stderr = stderr.encode()
        result = self.run(args, data, cwd)
        assert (result.returncode,result.stdout)==(status,stdout),(args,result.returncode,result.stdout,result.stderr,stdout)
        if stderr is not None: assert result.stderr==stderr,(args,result.stderr)
        return result
    def file(self, name, data):
        path = self.dir/name
        path.write_bytes(data.encode() if isinstance(data,str) else data)
        return path
