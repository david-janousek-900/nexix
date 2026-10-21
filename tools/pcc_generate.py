"""Bootstrap the retained PCC grammar with the checked native parser skeleton."""
from pathlib import Path
import os
import subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/gen-pcc';OUT.mkdir(parents=True,exist_ok=True)
subprocess.run([str(ROOT/'build/host-v7-yacc'),str(ROOT/'user/pcc/cgram.y')],cwd=OUT,
    env=dict(os.environ,NX_YACCPAR=str(ROOT/'rootfs/usr/lib/yaccpar')),check=True)
(OUT/'cgram.c').write_bytes((OUT/'y.tab.c').read_bytes())
