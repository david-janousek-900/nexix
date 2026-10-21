"""Check retained operator metadata, target widths and production IR under sanitizers."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'reference/v7/usr/src/cmd/mip/common').read_text()
native=(root/'user/mip/common.c').read_text()
# Compare every original operator, display name and flag expression independently.
pattern=r'^\s*(.+?),\s*"([^"]*)",\s*([^,\n]+),?\s*$'
original=re.findall(pattern,source[source.index('indope[]'):source.index('mkdope(){')],re.M)
for op,name,flags in original:
    assert '{ '+op.strip()+', "'+name+'", '+flags.strip()+' }' in native,(op,name,flags)
assert len(original)>70,len(original)
with tempfile.TemporaryDirectory(prefix='nx-mip-') as temp:
    binary=str(Path(temp)/'common')
    subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-fno-pie','-no-pie','-Iuser/mip',
                    'user/mip/common.c','tests/mip-common.c','-o',binary],cwd=root,check=True)
    subprocess.run([binary],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',
                                   UBSAN_OPTIONS='halt_on_error=1'),check=True)
print(f'PASS: all {len(original)} supplied mip operator records preserved')
