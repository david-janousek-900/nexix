"""Sanitize the production editor's resize, command, filename and BRE boundaries."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--cc',default='cc');args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='nx-ed-') as temp:
        binary=Path(temp)/'ed'
        flags=['-std=gnu89','-O1','-g','-fsanitize=address,undefined','-funsigned-char','-fcommon',
            '-fno-pie','-no-pie','-fno-strict-aliasing','-Wno-implicit-int','-Wno-implicit-function-declaration',
            '-Wno-incompatible-pointer-types','-Wno-int-conversion','-Wno-return-type','-Wno-return-mismatch',
            '-include',str(root/'user/v7/compat.h'),'-I'+str(root/'tests/ed')]
        subprocess.run(shlex.split(args.cc)+flags+[str(root/'user/v7/cmd/ed.c'),str(root/'tests/ed/tty.c'),'-lcrypt','-o',str(binary)],check=True)
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
        def run(script,expected='',arguments=('-',),status=0):
            result=subprocess.run([str(binary),*arguments],input=script,capture_output=True,text=True,cwd=temp,env=env,timeout=10)
            assert (result.returncode,result.stdout,result.stderr)==(status,expected,''),(script[:100],result.returncode,result.stdout,result.stderr)
        run('a\nhello\nworld\n.\n1,$p\nw result\nq\n','hello\nworld\n')
        assert (Path(temp)/'result').read_bytes()==b'hello\nworld\n'
        lines=''.join(f'line{i:03d}\n' for i in range(600))
        run('a\n'+lines+'.\n1,600t0\nw copied\nq\n')
        assert (Path(temp)/'copied').read_text()==lines*2
        run('1,$s/line/L\\\nX/\nw expanded\nq\n',arguments=('-', 'copied'))
        assert (Path(temp)/'expanded').read_text()==(''.join(f'L\nX{i:03d}\n' for i in range(600)))*2
        run('a\none\n.\n999-998p\nQ\n','one\n')
        run('a\nabc\n.\n1s/\\(\\)\\1*/X/\n1p\nQ\n','Xabc\n')
        run('a\nabc\n.\n1s/x*/Y/g\n1p\nQ\n','YaYbYcY\n')
        for command in ['999999999999999999999p','1s/\\1/x/','1s/\\(\\1\\)/x/','1s/'+'.'*140+'/x/','w '+'a'*100]:
            run('a\nx\n.\n'+command+'\nQ\n','?\n')
        result=subprocess.run([str(binary),'x'*100],capture_output=True,text=True,env=env)
        assert result.returncode==1 and result.stderr=='ed: filename too long\n' and not result.stdout,result
        run('',expected='?Input not tty\n',arguments=('-x',),status=1)
    print('PASS: ed ASan/UBSan resize/copy/substitution, native line tables, file persistence, zero-width/back-reference regex and input bounds')


if __name__=='__main__':main()
