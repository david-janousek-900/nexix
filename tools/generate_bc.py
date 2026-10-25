"""Bootstrap natively reproducible bc parser using the retained V7 yacc port."""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='nx-bc-yacc-') as temp:
    folder=Path(temp)
    (folder/'bc.y').write_bytes((ROOT/'user/bc/bc.y').read_bytes())
    env=dict(os.environ,NX_YACCPAR=str(ROOT/'rootfs/usr/lib/yaccpar'))
    subprocess.run([str(ROOT/'build/host-nx-yacc'),'bc.y'],cwd=folder,env=env,check=True)
    output=ROOT/'build/bc'
    output.mkdir(exist_ok=True)
    (output/'bc.c').write_bytes((folder/'y.tab.c').read_bytes())
