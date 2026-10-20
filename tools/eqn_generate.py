"""Generate the retained equation grammar using the V7 host bootstrap yacc."""
import os,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];name=sys.argv[1];out=root/'build'/('gen-'+name);out.mkdir(exist_ok=True)
env=dict(os.environ,NX_YACCPAR=str(root/'reference/v7/usr/lib/yaccpar'))
subprocess.run([str(root/'build/host-v7-yacc'),'-d',str(root/'user'/name/'e.y')],cwd=out,env=env,check=True)
(out/'e.c').write_text((out/'y.tab.c').read_text())
(out/'e.def').write_text((out/'y.tab.h').read_text())
