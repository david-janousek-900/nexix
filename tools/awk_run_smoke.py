"""Installed /bin/awk on the checked fixtures: exact output, errors, status and files."""
import sys
from pathlib import Path
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
F=Path('build/fixtures-awkrun')
cases=F.joinpath('cases').read_text().split()
art={}
# V7 names are at most 14 bytes: fixtures are numbered.
for i,c in enumerate(cases):
    for s in ('awk','in','out','err','status'):
        art[f'/tmp/k{i}.{s[0]}']=f'fixtures-awkrun/{c}.{s}'
    for kind in 'ab':
        for f in sorted(F.glob(f'{c}.{kind}[0-9]')):
            art[f'/tmp/k{i}.{f.suffix[1:]}']=f'fixtures-awkrun/{f.name}'
def run(g,t,d):
    cmd=lambda line,**k:g.command(line,terminal=t,**k)
    cmd(f'mkdir {d}; cd {d}')
    for i,c in enumerate(cases):
        opts=F.joinpath(c+'.opts').read_text().strip();operands=F.joinpath(c+'.operands').read_text().strip()
        for f in sorted(F.glob(f'{c}.b[0-9]')):
            cmd(f'cp /tmp/k{i}.{f.suffix[1:]} {F.joinpath(f.name+".name").read_text().strip()}')
        cmd(f'awk {opts} -f /tmp/k{i}.a {operands} < /tmp/k{i}.i > out 2> err; echo $? > status')
        cmd(f'cmp out /tmp/k{i}.o; cmp err /tmp/k{i}.e; cmp status /tmp/k{i}.s')
        for f in sorted(F.glob(f'{c}.a[0-9]')):
            name=F.joinpath(f.name+'.name').read_text().strip()
            cmd(f'cmp {name} /tmp/k{i}.{f.suffix[1:]}; rm {name}')
    cmd("awk; echo $?",stdout="awk: Usage: awk [-f source | 'cmds'] [files]\n2\n")
    cmd("ls -l /bin/awk | awk '{ print $1 }'",stdout="-rwxr-xr-x\n")
    # A command-line program, shell pipelines and the installed manual page.
    cmd("echo 3 4 | awk '{ print $1 * $2 }'",stdout="12\n")
    cmd("awk 'BEGIN { FS = \":\" } $1 == \"root\" { print $7 }' /etc/passwd",stdout="/bin/sh\n")
    cmd("echo a b | awk '{ print $2 | \"cat\" }'",stdout="b\n")
    cmd("test -f /usr/man/man1/awk.1")
    cmd('cd /')
with private_guest(qemu,iso,ram,'awkrun',art,terminals=2,single_user=False) as g:
    login(g,'root','nexix')
    run(g,0,'/tmp/r')
    login(g,'bob','bob',line=1,prompt='$ ')
    run(g,1,'/tmp/u')
print(f'PASS: {ram} installed awk: {len(cases)} interpreter, printf, redirection, file, pipe, regex, program and error fixtures, command-line use as root and ordinary user')
