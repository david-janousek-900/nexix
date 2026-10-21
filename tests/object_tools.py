"""Real ELF and archive fixtures plus sanitizer checks of production tools."""
from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[1]


def fixtures(folder):
    source=folder/'long_source.c'
    source.write_text('extern int unresolved_external; int initialized_global=7; int zero_global; '
                      'static int local_data=3; const char read_only_global[]="abc"; '
                      'int weak_function(void) __attribute__((weak)); '
                      'int weak_function(void){return local_data;} '
                      'int function_with_a_very_long_symbol_name(void){return unresolved_external;}\n')
    obj=folder/'object_with_a_very_long_member_name.o'
    subprocess.run(['gcc','-fcommon','-fno-pie','-fno-asynchronous-unwind-tables','-c',str(source),'-o',str(obj)],check=True)
    archive=folder/'native.a';subprocess.run(['ar','rcs',str(archive),str(obj)],check=True)
    data=obj.read_bytes(); name=obj.name.encode()
    def member(label,body):
        return label.ljust(16,b' ')+b'0'.ljust(12,b' ')+b'0'.ljust(6,b' ')*2+b'644'.ljust(8,b' ')+str(len(body)).encode().ljust(10,b' ')+b'`\n'+body+(b'\n' if len(body)%2 else b'')
    (folder/'bsd.a').write_bytes(b'!<arch>\n'+member(b'#1/'+str(len(name)).encode(),name+data))
    # A historical container carrying an ELF member exercises archive reading;
    # it does not claim PDP-11 object-code compatibility.
    n=len(data)
    hdr=b'fixture.o'.ljust(14,b'\0')+b'\0'*6+struct.pack('<H',0o644)+struct.pack('<HH',n>>16,n&65535)
    (folder/'historical.a').write_bytes(struct.pack('<H',0o177545)+hdr+data+(b'\0' if n%2 else b''))
    bads={'truncated':data[:63],'machine':data[:18]+b'\x03\x00'+data[20:],
          'archive-short':archive.read_bytes()[:-5],'string-offset':None}
    corrupt=bytearray(data); shoff=struct.unpack_from('<Q',data,40)[0]; shnum=struct.unpack_from('<H',data,60)[0]
    for i in range(shnum):
        entry=shoff+i*64
        if struct.unpack_from('<I',data,entry+4)[0]==2:
            symoff=struct.unpack_from('<Q',data,entry+24)[0]
            struct.pack_into('<I',corrupt,symoff+24,0xffffffff)
            break
    bads['string-offset']=bytes(corrupt)
    corrupt=bytearray(data);struct.pack_into('<Q',corrupt,shoff+64+24,0xffffffffffffffff)
    bads['section-range']=bytes(corrupt)
    for name,content in bads.items(): (folder/name).write_bytes(content)
    return obj,bads


def check(tool,binary,folder):
    obj,bads=fixtures(folder)
    def run(*args):
        # LSan's ptrace dependency is unavailable in the managed sandbox.
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
        result=subprocess.run([str(binary),*map(str,args)],capture_output=True,text=True,timeout=5,env=env)
        assert result.returncode>=0 and 'AddressSanitizer' not in result.stderr and 'runtime error:' not in result.stderr,result
        return result
    if tool=='nm':
        result=run('-g',obj);assert result.returncode==0,result.stderr
        for row in (' D initialized_global',' C zero_global',' R read_only_global',' W weak_function',' U unresolved_external'):
            assert row in result.stdout,(row,result.stdout)
        assert 'local_data' not in result.stdout and 'function_with_a_very_long_symbol_name' in result.stdout
        assert run('-u',obj).stdout=='unresolved_external\n'
        normal=run('-g',obj).stdout.splitlines()
        assert run('-gr',obj).stdout.splitlines()==list(reversed(normal))
        assert run('-gp',obj).returncode==0
        for archive in ('native.a','bsd.a','historical.a'):
            r=run('-go',folder/archive)
            assert r.returncode==0 and 'function_with_a_very_long_symbol_name' in r.stdout,(archive,r)
        for name in bads:
            r=run(folder/name);assert r.returncode!=0,(name,r.stdout)
        assert run('-z').returncode==2
        assert run(folder/'missing',obj).returncode==1
    if tool=='size':
        values=list(map(int,subprocess.check_output(['size','-B',str(obj)],text=True).splitlines()[1].split()[:3]))
        text,data,bss=values;total=sum(values)
        line=f'{text}+{data}+{bss} = {total}b = 0{total:o}b\n'
        r=run(obj);assert r.returncode==0 and r.stdout==line,r
        assert run(folder/'missing',obj).returncode==1
        for archive,membername in (('native.a',obj.name),('bsd.a',obj.name),('historical.a','fixture.o')):
            r=run(folder/archive)
            assert r.returncode==0 and r.stdout==f'{folder/archive}({membername}): '+line,r
        for name in bads:
            if name=='string-offset':continue # Symbol names do not affect sizes.
            r=run(folder/name);assert r.returncode==1,(name,r)
        assert run('-z').returncode==2
        (folder/'expected.txt').write_text(line)
        asm=folder/'exec.S';asm.write_text('.global _start\n.text\n_start: nop; ret\n.data\n.quad 7\n.bss\n.zero 64\n.section .note.GNU-stack,"",@progbits\n')
        subprocess.run(['gcc','-c',str(asm),'-o',str(folder/'exec.o')],check=True)
        subprocess.run(['ld','-T',str(ROOT/'user/user.ld'),str(folder/'exec.o'),'-o',str(folder/'exec.elf')],check=True)
        raw=bytearray((folder/'exec.elf').read_bytes())
        phoff=struct.unpack_from('<Q',raw,32)[0];phnum=struct.unpack_from('<H',raw,56)[0]
        sizes=[0,0,0]
        for i in range(phnum):
            p=struct.unpack_from('<IIQQQQQQ',raw,phoff+i*56)
            if p[0]==1:
                sizes[1 if p[1]&2 else 0]+=p[5];sizes[2]+=p[6]-p[5]
        struct.pack_into('<Q',raw,40,0);struct.pack_into('<HHH',raw,58,0,0,0)
        (folder/'sectionless').write_bytes(raw)
        a,b,c=sizes;s=sum(sizes);expected=f'{a}+{b}+{c} = {s}b = 0{s:o}b\n'
        r=run(folder/'sectionless');assert r.returncode==0 and r.stdout==expected,r
        (folder/'segment.txt').write_text(expected)
    if tool=='lorder':
        a=folder/'a.c';b=folder/'b.c'
        a.write_text('extern int b_value,b_common; extern const int b_const; extern int b_func(void); '
                     'int a_func(void){return b_value+b_common+b_const+b_func();}\n')
        b.write_text('int b_value=1; int b_common; const int b_const=2; int b_func(void){return 3;}\n')
        for src in (a,b):
            subprocess.run(['gcc','-fcommon','-fno-pie','-c',str(src),'-o',str(src.with_suffix('.o'))],check=True)
        aa=a.with_suffix('.o');bb=b.with_suffix('.o')
        r=run(aa,bb);expected=f'{aa} {aa}\n{bb} {bb}\n'+f'{aa} {bb}\n'*4
        assert r.returncode==0 and r.stdout==expected,r
        assert run(aa).stdout==f'{aa} {aa}\n'*2
        assert run().stdout=='usage: lorder file ...\n'
        assert run(folder/'absent').returncode==1
        assert run('with space.o').returncode==2
        for name in bads:
            assert run(folder/name).returncode==1,name
        for archive,member in (('native.a',obj.name),('bsd.a',obj.name),('historical.a','fixture.o')):
            label=f'{folder/archive}({member})'
            r=run(folder/archive);assert r.returncode==0 and r.stdout==f'{label} {label}\n',r
        cycle=folder/'cycle.c';cycle.write_text('extern int a_func(void);int b_func(void){return a_func();}\n')
        subprocess.run(['gcc','-fno-pie','-c',str(cycle),'-o',str(folder/'cycle.o')],check=True)
        cc=folder/'cycle.o';r=run(aa,cc)
        assert r.returncode==0 and set(r.stdout.splitlines())=={f'{aa} {aa}',f'{cc} {cc}',f'{aa} {cc}',f'{cc} {aa}'},r
    return obj,bads


if __name__=='__main__':
    tool=sys.argv[1]
    with tempfile.TemporaryDirectory(prefix='nx-object-tools-') as temp:
        folder=Path(temp);binary=folder/tool
        subprocess.run(['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
            '-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/toolchain',
            'user/v7/cmd/'+tool+'.c','user/toolchain/object.c','-o',str(binary)],cwd=ROOT,check=True)
        check(tool,binary,folder)
        # Keep exact fixtures for native guest acceptance.
        out=ROOT/'build'/('fixtures-'+tool);out.mkdir(exist_ok=True)
        for p in folder.iterdir():
            if p.is_file() and p!=binary:(out/p.name).write_bytes(p.read_bytes())
    print('PASS: '+tool+' production source with ASan/UBSan, ELF symbols and GNU/BSD/V7 archives; malformed-input rejection')
