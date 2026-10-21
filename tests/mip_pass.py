"""Shared pass framing, failures and a parser-independent analysis consumer."""
from pathlib import Path
import os
import struct
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-mip';OUT.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
def inspect(data):
    assert data[:16]==b'NXMPS\0\2\0\1\0\0\0\0\0\0\0'
    offset=16;stats=dict(nodes=0,expressions=0,initializers=0,names=0);trees=0
    while True:
        kind,bits,length=struct.unpack_from('<III',data,offset);offset+=12
        payload=data[offset:offset+length];offset+=length
        assert len(payload)==length
        if kind==0:
            assert bits==length==0 and offset==len(data)
            break
        if kind==1:
            assert not bits and b'\0' not in payload
            continue
        assert kind in (2,3) and (bits in (8,16,32,64,128) if kind==3 else bits==0)
        assert payload[:8]==b'NXMIP\0\2\0'
        count,root=struct.unpack_from('<II',payload,8);at=16;nodes=[]
        for _ in range(count):
            fields=struct.unpack_from('<IQIIQQQ10sQIIIIIIH',payload,at);at+=88
            name=payload[at:at+fields[-1]];at+=len(name)
            nodes.append((fields[2],fields[3],bool(name)))
        assert at==len(payload) and root<count
        seen=set();todo=[root]
        while todo:
            i=todo.pop()
            if i==0xffffffff or i in seen:continue
            assert i<len(nodes);seen.add(i);todo.extend(nodes[i][:2])
        stats['nodes']+=len(seen);stats['names']+=sum(nodes[i][2] for i in seen)
        stats['initializers' if bits else 'expressions']+=1;trees+=1
    return stats,trees
def report(stats):
    return (' '.join(f'{key}={value}' for key,value in stats.items())+'\n').encode()
with tempfile.TemporaryDirectory(prefix='nx-mip-pass-') as tmp:
    d=Path(tmp);binary=d/'analysis'
    subprocess.run(['cc','-D_POSIX_C_SOURCE=200809L','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/mip',
        'user/mip/common.c','user/mip/trees.c','user/mip/reader.c','tests/mip-pass.c','-o',str(binary)],cwd=ROOT,check=True)
    subprocess.run([str(binary),str(OUT/'alternate.pass'),str(d/'scratch')],env=ENV,check=True)
    stats,trees=inspect((OUT/'alternate.pass').read_bytes());assert trees==4
    assert stats==dict(nodes=7,expressions=3,initializers=1,names=2)
    result=subprocess.run([str(binary),'--check',str(OUT/'alternate.pass')],capture_output=True,env=ENV,check=True)
    assert result.stdout==report(stats)
    # These are real retained-PCC producer records, including LP64 pointers,
    # static initializers, calls, fields, control, floating values and STASG.
    passes=sorted((ROOT/'build/fixtures-pcc').glob('*.pass'));assert passes
    total=0
    for path in passes:
        stats,trees=inspect(path.read_bytes());total+=trees
        result=subprocess.run([str(binary),'--check',str(path)],capture_output=True,env=ENV,check=True)
        assert result.stdout==report(stats) and not result.stderr,(path,result)
        if path.stem=='integer':(OUT/'pcc-analysis').write_bytes(result.stdout)
    assert total>100,total
print('PASS: shared mip stream failures, alternate producer and independent PCC analysis consumer')
