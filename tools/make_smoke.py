"""Boot installed V7 make: graph rebuilds, native processes, wildcards/ELF."""
from pathlib import Path
import sys
from private_guest import private_guest
from multiuser_acceptance import login

qemu,iso,ram=sys.argv[1:4]
artifacts={'/tmp/stamp':'make-time.elf','/tmp/lib.a':'make-lib.a'}
def fixture(name,text):
    artifact='make-'+name;Path('build',artifact).write_text(text);artifacts['/tmp/'+name]=artifact
fixture('main.mk','MSG=file\nall: output\n\t@echo $(MSG):$@:$?\noutput: input\n\t@cat input > $@\n')
fixture('input','alpha\n')
fixture('fail.mk','bad:\n\tfalse\nnext:\n\t@echo continued\n')
fixture('ignore.mk','bad:\n\t-false\n\t@echo survived\n')
fixture('double.mk','all::\n\t@echo first\nall::\n\t@echo second\n')
fixture('suffix.mk','.SUFFIXES: .src .dst\n.src.dst:\n\t@cp $< $@\nresult.dst:\n')
fixture('result.src','inferred\n')
fixture('wild.mk','all: *.src\n\t@echo $?\n')
fixture('inline.mk','VAR = hel\\\n lo\nall: ; @echo $(VAR) # literal\n')
fixture('one.mk','NAME=multi\n')
fixture('two.mk','all:\n\t@echo $(NAME)\n')
fixture('archive.mk','member: lib.a(make-member.o)\n\t@echo member-ok\nsymbol: lib.a((exported))\n\t@echo symbol-ok\n')
fixture('interrupt.mk','victim:\n\t@echo data > $@; echo MAKE_STARTED; sleep 20\n')
fixture('precious.mk','.PRECIOUS: victim\nvictim:\n\t@echo data > $@; echo MAKE_STARTED; sleep 20\n')
fixture('bad.mk','all: all\n')
fixture('recursive.mk','A=$(A)\nall:\n\techo $(A)\n')
fixture('huge.mk','A='+('x'*1200)+'\nall:\n\techo $(A)$(A)$(A)\n')
fixture('deep.mk',''.join(f'n{i}: n{i+1}\n' for i in range(30))+'n30:\n\t@echo deep-ok\n')
with private_guest(qemu,iso,ram,'make',artifacts,2,single_user=False) as guest:
    login(guest,'root','nexix')
    guest.command('cd /tmp')
    guest.command('/tmp/stamp 100 input')
    guest.command('make -q -f main.mk output',status=255)
    guest.command('make -f main.mk',stdout='file:all:output\n')
    guest.command('cat output',stdout='alpha\n')
    guest.command('make -f main.mk output',stdout="`output' is up to date.\n")
    guest.command('make -q -f main.mk output')
    guest.command('/tmp/stamp 2000000000 input')
    guest.command('make -n -f main.mk output',stdout='cat input > output\n')
    guest.command('echo beta > input')
    guest.command('/tmp/stamp 100 output')
    guest.command('make -f main.mk output')
    guest.command('cat output',stdout='beta\n')
    guest.command('make -f main.mk MSG=argument',stdout='argument:all:output\n')
    guest.command('make -t -f main.mk all',stdout='touch(all)\n')
    guest.command('rm all')
    guest.command('make -f fail.mk bad',stdout='false\n*** Error code 1\n\nStop.\n',status=1)
    guest.command('make -k -f fail.mk bad next',stdout='false\n*** Error code 1\ncontinued\n',status=1)
    guest.command('make -i -f fail.mk bad',stdout='false\n*** Error code 1 (ignored)\n')
    guest.command('make -f ignore.mk bad',stdout='false\n*** Error code 1 (ignored)\nsurvived\n')
    guest.command('make -f double.mk',stdout='first\nsecond\n')
    guest.command('make -q -f double.mk',status=255)
    guest.command('make -f suffix.mk result.dst')
    guest.command('cat result.dst',stdout='inferred\n')
    # A real V7 directory read expands a wildcard into dependencies.
    guest.command('make -f wild.mk',stdout='result.src\n')
    guest.command('make -f inline.mk',stdout='hel lo # literal\n')
    guest.command('make -f one.mk -f two.mk',stdout='multi\n')
    guest.command('make -f archive.mk member',stdout='member-ok\n')
    guest.command('make -f archive.mk symbol',stdout='symbol-ok\n')
    guest.command('make -f deep.mk',stdout='deep-ok\n')
    guest.command('make -f bad.mk',stdout='Make: dependency cycle at all.  Stop.\n',status=1)
    guest.command('make -f recursive.mk',stdout='Make: infinitely recursive macro?.  Stop.\n',status=1)
    guest.command('make -f huge.mk',stdout='Make: expanded line too long.  Stop.\n',status=1)
    for description,exists in [('interrupt.mk',False),('precious.mk',True)]:
        guest.send('make -f '+description+'\n')
        guest.expect('MAKE_STARTED\r\n')
        guest.send('\x03');guest.expect('# ')
        guest.command('test -f victim',status=0 if exists else 1)
    guest.command('/tmp/stamp 2000000000 input')
    guest.command('/tmp/stamp 100 output')
    login(guest,'alice','alice',line=1,prompt='$ ')
    guest.command('cd /tmp',terminal=1,prompt=r'\$ ')
    guest.command('make -t -f main.mk output',terminal=1,prompt=r'\$ ',
                  stdout='touch(output)\nCannot touch output\nMake: cannot touch target.  Stop.\n',status=1)

print(f'PASS: {ram} make timestamp rebuilds, macros, suffix/wildcard rules, native archive/ELF, flags and failures')
