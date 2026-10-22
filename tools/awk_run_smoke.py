"""Native awk interpreter (private build until task 176 installs it) on the checked fixtures."""
import sys
from pathlib import Path
from private_guest import private_guest
from multiuser_acceptance import login
qemu,iso,ram=sys.argv[1:4]
F=Path('build/fixtures-awkrun')
cases=F.joinpath('cases').read_text().split()
art={'/tmp/awk':'v7-awk.elf'}
# V7 names are at most 14 bytes: fixtures are numbered.
for i,c in enumerate(cases):
    for s in ('awk','in','out','err','status'):
        art[f'/tmp/k{i}.{s[0]}']=f'fixtures-awkrun/{c}.{s}'
def run(g,t,d):
    cmd=lambda line,**k:g.command(line,terminal=t,**k)
    cmd(f'mkdir {d}; cd {d}')
    for i,c in enumerate(cases):
        opts=F.joinpath(c+'.opts').read_text().strip();operands=F.joinpath(c+'.operands').read_text().strip()
        cmd(f'/tmp/awk {opts} -f /tmp/k{i}.a {operands} < /tmp/k{i}.i > out 2> err; echo $? > status')
        cmd(f'cmp out /tmp/k{i}.o; cmp err /tmp/k{i}.e; cmp status /tmp/k{i}.s')
    cmd("/tmp/awk; echo $?",stdout="awk: Usage: awk [-f source | 'cmds'] [files]\n2\n")
    cmd('cd /')
with private_guest(qemu,iso,ram,'awkrun',art,terminals=2,single_user=False) as g:
    login(g,'root','nexix')
    run(g,0,'/tmp/r')
    login(g,'bob','bob',line=1,prompt='$ ')
    run(g,1,'/tmp/u')
print(f'PASS: {ram} native awk interpreter: {len(cases)} data-model, field, array, arithmetic, string, control and error fixtures as root and ordinary user')
