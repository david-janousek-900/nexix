"""Generate awk's parser, scanner, token names and procedure table on the host.

The steps follow the supplied makefile: yacc -d, awk.h from y.tab.h, lex,
tokenscript's ed edits of token.c, and proc > proctab.c."""
import os,re,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];src=root/'user/awk';out=root/'build/gen-awk';out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,NX_YACCPAR=str(root/'rootfs/usr/lib/yaccpar'),NX_LEXLIB=str(root/'rootfs/usr/lib/lex'))
cc=sys.argv[1] if len(sys.argv)>1 else 'cc'
def tokens(header,template):
    """tokenscript: "# define NAME n" lines become "NAME", n, inside tok[]."""
    rows=[]
    for line in header.splitlines():
        line=re.sub(r'# *define *','',line,count=1);line='"'+line;line=line.replace(' ','", ',1)+','
        rows.append(line)
    lines=template.splitlines()
    start=next(i for i,l in enumerate(lines) if re.search(r'= *\{$',l))
    end=next(i for i in range(start+1,len(lines)) if lines[i]=='};')
    return '\n'.join(lines[:start+1]+rows+lines[end:])+'\n'
def write(name,data):
    path=out/name
    if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)
# Generators read copies, so line directives name awk.g.y as a guest build does.
for name in ('awk.g.y','awk.lx.l'):write(name,(src/name).read_bytes())
subprocess.run([str(root/'build/host-nx-yacc'),'-d','awk.g.y'],cwd=out,env=env,check=True,stdout=subprocess.DEVNULL)
write('awk.h',(out/'y.tab.h').read_bytes())
subprocess.run([str(root/'build/host-v7-lex'),'-t','awk.lx.l'],cwd=out,env=env,check=True,stdout=open(out/'awk.lx.c.tmp','wb'))
os.replace(out/'awk.lx.c.tmp',out/'awk.lx.c')
write('token.c',tokens((out/'y.tab.h').read_text(),(src/'token.c').read_text()).encode())
subprocess.run([cc,'-std=gnu89','-w','-I',str(out),'-I',str(src),str(src/'proc.c'),str(out/'token.c'),'-o',str(out/'proc')],check=True)
write('proctab.c',subprocess.run([str(out/'proc')],check=True,capture_output=True).stdout)
