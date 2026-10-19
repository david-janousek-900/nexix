"""Exercise the production V7 grep parser and matcher with sanitizers."""
import os
from pathlib import Path
import subprocess
import tempfile


def cases(binary, env, directory):
    path=Path(directory)/'text';path.write_text('alpha\nbeta\nALPHA\nabba\n')
    def check(args, output='', status=0, data=None):
        result=subprocess.run([str(binary),*args],input=data,text=True,capture_output=True,env=env,timeout=5)
        assert (result.returncode,result.stdout)==(status,output),(args,result)
        assert 'runtime error' not in result.stderr and 'AddressSanitizer' not in result.stderr,result.stderr
    check(['alpha',str(path)],'alpha\n')
    check(['-y','alpha',str(path)],'alpha\nALPHA\n')
    check(['-n','-v','alpha',str(path)],'2:beta\n3:ALPHA\n4:abba\n')
    check(['-c','a',str(path)],'3\n')
    check(['-s','a',str(path)])
    check(['-l','a',str(path)],str(path)+'\n')
    check([r'^\(ab\)\1$',str(path)],status=1)
    check([r'^\(a\)bb\1$',str(path)],'abba\n')
    check([r'\(\)\1*',''],status=2)
    check([r'^\(\)\1*$',str(path)],status=1)
    check(['[a-z]*a$',str(path)],'alpha\nbeta\nabba\n')
    check(['nomatch',str(path)],status=1)
    check(['a',str(path)+'missing'],status=2)
    check(['a'],'a\n',data='a\nunterminated a')
    for pattern in ['\\','[',r'\(',r'\1',r'\(a\1\)',r'\(\(a\)\1', '.'*256, '['+'a'*2000, '[é]']:
        check([pattern,str(path)],status=2)
    check(['-y','['+'a'*2000,str(path)],status=2)
    for pattern in [r'\(a*\)\1*',r'\(\)\1*','.*.*.*x','a*a*a*b','[a-z]*$']:
        check([pattern],status=0 if pattern != '.*.*.*x' else 1,output='aab\n' if pattern != '.*.*.*x' else '',data='aab\n')


def main():
    with tempfile.TemporaryDirectory() as directory:
        binary=Path(directory)/'grep'
        subprocess.run(['cc','-std=gnu89','-funsigned-char','-fno-builtin','-fno-pie','-no-pie','-w','-fsanitize=address,undefined','-g','user/v7/cmd/grep.c','-o',str(binary)],check=True)
        cases(binary,dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),directory)
    print('PASS: grep ASan/UBSan flags, BRE bounds/backreferences, empty captures and status codes')

if __name__=='__main__':main()
