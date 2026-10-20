"""Regenerate V7 formatter data from C sources, never PDP-11 object offsets."""
import argparse,re,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cc',default='cc');p.add_argument('--output',default='build/roff-data');a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
base=root/'reference/v7/usr/src/cmd/troff'
with tempfile.TemporaryDirectory(prefix='nx-roff-tables-') as td:
 td=Path(td)
 for source in sorted((base/'term').glob('tab*.c')):
  text=source.read_text().replace('} t {','} t = {')
  # Includes in historical tables are local character initializer lists.
  text=re.sub(r'#include "([^"]+)"',lambda m:(base/'term'/m[1]).read_text(),text)
  driver='''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static void u32(uint32_t x){unsigned char b[4]={x,x>>8,x>>16,x>>24};fwrite(b,1,4,stdout);}
int main(void){int i;char **p;int *n=&t.bset;fwrite("NXTERM1",1,8,stdout);
for(i=0;i<9;i++)u32(n[i]);
for(p=&t.twinit;p<&t.codetab[224];p++){const char *s=*p?*p:"";u32(strlen(s));fwrite(s,1,strlen(s),stdout);}return ferror(stdout);}
'''
  c=td/'table.c';exe=td/'table';c.write_text(text+driver)
  subprocess.run([a.cc,'-w','-o',str(exe),str(c)],check=True)
  with (out/source.stem).open('wb') as f:subprocess.run([str(exe)],stdout=f,check=True)
 for source in sorted((base/'font').glob('ft*.c')):
  text=re.sub(r'(char\s+\w+\s*\[[^]]*\])\s*\{',r'\1 = {',source.read_text());name=re.search(r'char\s+(\w+)\s*\[',text)[1]
  c=td/'font.c';exe=td/'font';c.write_text('#include <stdio.h>\n'+text+f'\nint main(void){{return fwrite({name},1,224,stdout)!=224;}}\n')
  subprocess.run([a.cc,'-w','-o',str(exe),str(c)],check=True)
  with (out/source.stem).open('wb') as f:subprocess.run([str(exe)],stdout=f,check=True)
(out/'stamp').touch()
