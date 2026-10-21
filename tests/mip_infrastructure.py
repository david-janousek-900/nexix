"""Shared mip services under sanitizers; independently decode the native pass."""
from pathlib import Path
import os
import struct
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/fixtures-mip';OUT.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
with tempfile.TemporaryDirectory(prefix='nx-mip-') as tmp:
    binary=Path(tmp)/'infra'
    subprocess.run(['cc','-D_POSIX_C_SOURCE=200809L','-std=c11','-DMIP_DEEP_COUNT=16384','-O1','-g','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/mip',
        'user/mip/common.c','user/mip/trees.c','user/mip/match.c','user/mip/allo.c','user/mip/diagnostics.c','tests/mip-infrastructure.c','-o',str(binary)],cwd=ROOT,check=True)
    subprocess.run([str(binary),str(OUT/'generated.s'),str(OUT/'pass.ir'),str(Path(tmp)/'diag')],env=ENV,check=True)
    data=(OUT/'pass.ir').read_bytes();assert data[:8]==b'NXMIP\0\1\0'
    assert struct.unpack_from('<II',data,8)==(3,2)
    fields=[];offset=16
    for i in range(3):
        f=struct.unpack_from('<IQIIQQQQIIIIIIH',data,offset);offset+=78+f[-1];fields.append(f)
    assert offset==len(data) and fields[0][0:5]==(4,5,0xffffffff,0xffffffff,19)
    assert fields[1][4]==23 and fields[2][0:4]==(6,5,0,1) and fields[0][-2]==42
    expected='''.text
.globl ir_value
.type ir_value,@function
ir_value:
 movq $19,%rax
 addq $23,%rax
 ret
'''
    assert (OUT/'generated.s').read_text()==expected
    subprocess.run(['as','--64',str(OUT/'generated.s'),'-o',str(OUT/'generated.o')],check=True)
    source=OUT/'driver.c';source.write_text('#include <stdio.h>\nextern long ir_value(void);int main(void){if(ir_value()!=42)return 1;puts("PASS: shared mip template pipeline");return 0;}\n')
    subprocess.run(['gcc','-std=c11','-O2','-ffreestanding','-fno-builtin','-fno-pie','-fno-stack-protector','-fno-asynchronous-unwind-tables','-mno-red-zone','-mno-avx','-nostdinc','-Ilibc/include','-Iinclude','-c',str(source),'-o',str(OUT/'driver.o')],cwd=ROOT,check=True)
print('PASS: mip shared services ASan/UBSan, independent pass decoding and GNU assembly consumer')
