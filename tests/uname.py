"""Run production uname option parsing/output with a deterministic API stub."""
import os
from pathlib import Path
import subprocess
import tempfile
with tempfile.TemporaryDirectory() as directory:
    root=Path(directory);stub=root/'identity.c';binary=root/'uname'
    stub.write_text('''#include <sys/utsname.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
int uname(struct utsname *p) {
 if(getenv("UNAME_FAIL")) { errno=EIO; return -1; }
 memset(p,0,sizeof *p);strcpy(p->sysname,"Nexix");strcpy(p->nodename,"nexix.localdomain");
 strcpy(p->release,"0.2.0");strcpy(p->version,"development");strcpy(p->machine,"x86_64");return 0;
}
''')
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fno-pie','-no-pie','-fsanitize=address,undefined','user/v7/cmd/uname.c',str(stub),'-o',str(binary)],check=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
    cases=[([], 'Nexix\n'),(['--'],'Nexix\n'),(['-a'],'Nexix nexix.localdomain 0.2.0 development x86_64\n'),(['-m','-r'],'0.2.0 x86_64\n'),(['-ssn'],'Nexix nexix.localdomain\n')]
    for flag,value in zip('snrvm',['Nexix','nexix.localdomain','0.2.0','development','x86_64']):cases.append((['-'+flag],value+'\n'))
    for args,expected in cases:
        result=subprocess.run([str(binary),*args],env=env,text=True,capture_output=True,timeout=5)
        assert (result.returncode,result.stdout,result.stderr)==(0,expected,''),result
    for args in [['-z'],['-'],['file'],['--','file'],['-a','extra']]:
        result=subprocess.run([str(binary),*args],env=env,text=True,capture_output=True,timeout=5)
        assert (result.returncode,result.stdout,result.stderr)==(2,'','usage: uname [-asnrvm]\n'),result
    result=subprocess.run([str(binary)],env=dict(env,UNAME_FAIL='1'),text=True,capture_output=True,timeout=5)
    assert result.returncode==1 and result.stdout=='' and 'uname:' in result.stderr,result
    with open('/dev/full','w') as output:
        result=subprocess.run([str(binary)],env=env,stdout=output,stderr=subprocess.PIPE,timeout=5)
        assert result.returncode==1,result
print('PASS: uname ASan/UBSan flags, canonical ordering, duplicates, invalid arguments and API/output failures')
