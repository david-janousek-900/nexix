"""Rendered VGA/ANSI screens and saved bytes for common traditional vi edits."""
from pathlib import Path
import sys,tempfile,time
from private_guest import private_guest
from vga_smoke import LocalConsole
from terminal_screen import TerminalScreen
qemu,iso,ram=sys.argv[1:4]
base=[f'line {n:02d} alpha beta gamma 0123456789' for n in range(1,13)]
big=[f'page line {n:03d} alpha beta' for n in range(1,81)]
wide=['','\talpha\tbeta','A'*79,'B'*80,'C'*81,'D'*159,'end']
cases=[
 ('dd','1Gdd',base,base[1:]),
 ('counted-dd','1G3dd',base,base[3:]),
 ('undo','1Gddu',base,base),
 ('yank-put','1Gyyp',base,[base[0],*base]),
 ('yank-before','2Gyy1GP',base,[base[1],*base]),
 ('counted-yank','1G2yyp',base,[base[0],*base[:2],*base[1:]]),
 ('join','1GJ',base,[base[0]+' '+base[1],*base[2:]]),
 ('x','1G0x',base,[base[0][1:],*base[1:]]),
 ('dw','1G0dw',base,[base[0][5:],*base[1:]]),
 ('D','1G05lD',base,[base[0][:5],*base[1:]]),
 ('r','1G0rX',base,['X'+base[0][1:],*base[1:]]),
 ('cw','1G0cwWORD\x1b',base,['WORD'+base[0][4:],*base[1:]]),
 ('C','1G0Cchanged\x1b',base,['changed',*base[1:]]),
 ('i','1G0iPRE \x1b',base,['PRE '+base[0],*base[1:]]),
 ('A','1GA POST\x1b',base,[base[0]+' POST',*base[1:]]),
 ('o','1Goadded\x1b',base,[base[0],'added',*base[1:]]),
 ('O','1GOadded\x1b',base,['added',*base]),
 ('split','1G05li\n\x1b',base,[base[0][:5],base[0][5:],*base[1:]]),
 ('dot','1G0xj0.',base,[base[0][1:],base[1][1:],*base[2:]]),
 ('char-put','1G0ylj0p',base,[base[0],base[1][0]+base[0][0]+base[1][1:],*base[2:]]),
 ('forward-search','1G/line 05\n0rX',base,[*base[:4],'X'+base[4][1:],*base[5:]]),
 ('backward-search','G?line 03\n0rX',base,[*base[:2],'X'+base[2][1:],*base[3:]]),
 ('repeat-search','1G/alpha\nn0rX',base,[base[0],'X'+base[1][1:],*base[2:]]),
 ('substitute',':%s/alpha/ALPHA/g\n',base,[s.replace('alpha','ALPHA') for s in base]),
 ('global-delete',':g/line 0[2468]/d\n',base,[s for n,s in enumerate(base,1) if n not in [2,4,6,8]]),
 ('range-delete',':2,4d\n',base,[base[0],*base[4:]]),
 ('shift','1G>>',base,['\t'+base[0],*base[1:]]),
 ('unshift','1G>><<',base,base),
 ('line-undo','1G0xxU',base,base),
 ('counted-change','1G2ccnew\x1b',base,['new',*base[2:]]),
 ('delete-last','Gdd',base,base[:-1]),
 ('delete-all','1GdG',base,[]),
 ('page-motion','\x06\x02\x04\x15G1G',big,big),
 ('wrapped-tabs','G1Gjjllkkhh',wide,wide),
]
def encoded(lines):return '\n'.join(lines)+('\n' if lines else '')
artifacts={}
for label,lines in [('base',base),('big',big),('wide',wide)]:
 name='vi-edit-'+label;Path('build',name).write_text(encoded(lines));artifacts['/tmp/'+label]=name
artifacts['/tmp/zzvga']='vi-edit-base';artifacts['/tmp/zzserial']='vi-edit-base'
for n,(_,_,_,expected) in enumerate(cases):
 name=f'vi-edit-exp{n}';Path('build',name).write_text(encoded(expected));artifacts[f'/tmp/exp{n}']=name

def rendered(lines):
    rows=[]
    for line in lines:
        text=line.expandtabs(8)
        rows.extend([text[i:i+80].rstrip() for i in range(0,len(text),80)] or [''])
    return rows or ['']

with tempfile.TemporaryDirectory(prefix='nx-vi-edit-') as temp:
 with private_guest(qemu,iso,ram,'vi-edit',artifacts,extra_args=['-vga','std','-qmp',f'unix:{temp}/qmp,server=on,wait=off']) as guest:
  screen=LocalConsole(guest,temp)
  try:
   guest.expect(r'root@nexix.localdomain# ')
   for backend in ['vga','serial']:
    term=TerminalScreen();offset=len(guest.buffers[0])
    if backend=='vga':guest.send('TERM=nexix vi /tmp/base < /dev/tty0 > /dev/tty0 2>&1; echo VI_EDIT_DONE\n');screen.expect('line 12 alpha')
    else:guest.send('TERM=ansi vi /tmp/base; echo VI_EDIT_DONE\n');guest.expect('12 lines')
    def send(keys):
     if backend=='vga':screen.type(keys)
     else:guest.send(keys)
    def snapshot():
     return screen.screen().split('\n') if backend=='vga' else term.screen()
    def check(label,lines):
     expected=rendered(lines);height=25 if backend=='vga' else 24
     expected=(expected+['~']*height)[:height-1];count=height-1
     deadline=time.monotonic()+3
     # Let output settle even when a motion should leave every cell unchanged.
     guest.pump(.1)
     while True:
      if backend=='serial':
       term.feed(guest.buffers[0][state[0]:]);state[0]=len(guest.buffers[0])
      rows=snapshot()
      if rows[:count]==expected[:count]:break
      if time.monotonic()>=deadline:raise AssertionError(f'{backend} {label}: expected {expected[:count]!r}, got {rows!r}')
      guest.pump(.05)
    state=[offset]
    check('initial',base)
    for keys in ['j','10l','k','h','l','j','j','k','k','0']:
     send(keys);check('motion '+keys,base)
    send(':set number\n');check('line numbers',[f'{n:6d}  '+s for n,s in enumerate(base,1)])
    send('j10lk0');check('numbered motion',[f'{n:6d}  '+s for n,s in enumerate(base,1)])
    send(':set nonumber\n');check('nonumber',base)
    for n,(label,keys,start,expected) in enumerate(cases):
     source='base' if start is base else 'big' if start is big else 'wide'
     send(f':e! /tmp/{source}\n1G');check(label+' initial',start)
     send(keys+'1G');check(label,expected)
     send(f':w! /tmp/{backend}{n}\n');check(label+' saved',expected)
    send(f':e! /tmp/zz{backend}\n1G0rZ');check('dirty quit setup',['Z'+base[0][1:],*base[1:]])
    send(':q\n');check('refuse dirty quit',['Z'+base[0][1:],*base[1:]])
    send('0rY');check('still editing after refused quit',['Y'+base[0][1:],*base[1:]])
    send('ZZ');guest.expect('VI_EDIT_DONE\r\n');guest.expect(r'root@nexix.localdomain# ')
    guest.command(f'cat /tmp/zz{backend}',stdout=encoded(['Y'+base[0][1:],*base[1:]]))
    for n in range(len(cases)):guest.command(f'cmp /tmp/{backend}{n} /tmp/exp{n}')
    print(f'PASS: {ram} {backend} {len(cases)} edit cases: rendered cells and exact saved files; EOF cells, number mode, dirty quit and ZZ',flush=True)
  finally:
   Path(f'build/vi-edit-{ram}.screen.txt').write_text(screen.screen());screen.close()
