"""Byte-exact independent V7 dump fixtures and sanitizer bounds."""
import os
from pathlib import Path
import subprocess
import tempfile


def expected(data,formats='o',start=0,base=8):
    def number(n,b,width):
        text={8:lambda:format(n,'o'),10:lambda:str(n),16:lambda:format(n,'x')}[b]()
        return text.zfill(width)[-width:]
    def char(c):
        if 32<=c<127:return '  '+chr(c)
        special={0:r' \0',8:r' \b',12:r' \f',10:r' \n',13:r' \r',9:r' \t'}
        return special.get(c,format(c,'03o'))
    widths={'o':6,'d':5,'x':4,'c':7,'b':7};width=max(widths[c] for c in formats)
    output='';previous=None;star=False
    for offset in range(0,len(data),16):
        chunk=data[offset:offset+16]
        if chunk==previous:
            if not star:output+='*\n';star=True
            continue
        previous=chunk;star=False
        padded=chunk+(b'\0' if len(chunk)%2 else b'')
        words=[padded[i]|padded[i+1]<<8 for i in range(0,len(padded),2)]
        for index,c in enumerate(formats):
            output+=number(start+offset,base,7)+' ' if index==0 else '\t'
            values=[]
            for word in words:
                if c=='c':value=char(word&255)+' '+char(word>>8)
                elif c=='b':value=f'{word&255:03o} {word>>8:03o}'
                else:value=number(word,{'o':8,'d':10,'x':16}[c],widths[c])
                values.append(' '*(width-widths[c])+value)
            output+=' '.join(values)+'\n'
    return output+number(start+len(data),base,7)+'\n'


def main():
    with tempfile.TemporaryDirectory() as directory:
        binary=Path(directory)/'od';path=Path(directory)/'bytes'
        subprocess.run(['cc','-std=gnu89','-funsigned-char','-fno-builtin','-fno-pie','-no-pie','-w','-fsanitize=address,undefined','-g','user/v7/cmd/od.c','-o',str(binary)],check=True)
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
        def check(data,args,want,status=0,pipe=False):
            path.write_bytes(data)
            result=subprocess.run([str(binary),*args,*([] if pipe else [str(path)])],input=data if pipe else None,capture_output=True,env=env,timeout=5)
            assert (result.returncode,result.stdout)==(status,want.encode()),(args,result,want)
            assert b'Sanitizer' not in result.stderr and b'runtime error' not in result.stderr,result.stderr
        for data in [b'',b'A',b'ABC\n',bytes(range(256)),b'\0'*49,b'x'*4097]:
            for formats in ['o','d','x','c','b','odxcb']:
                check(data,['-'+formats],expected(data,formats))
            check(data,[],expected(data),pipe=True)
        data=bytes(range(64));path.write_bytes(data)
        for offset,start,base in [('+10',8,8),('+10.',10,10),('+x10',16,16),('+0x10',16,16)]:
            result=subprocess.run([str(binary),str(path),offset],capture_output=True,env=env,timeout=5)
            assert result.returncode==0 and result.stdout.decode()==expected(data[start:],'o',start,base),result
        for arg in ['+9999999999999999999999.','+08','+10garbage']:
            result=subprocess.run([str(binary),str(path),arg],capture_output=True,env=env,timeout=5)
            assert result.returncode==1,result
        check(b'abc',['-z'],'',1)
    print('PASS: od ASan/UBSan independent byte/word formats, odd bytes, duplicate rows and offsets')

if __name__=='__main__':main()
