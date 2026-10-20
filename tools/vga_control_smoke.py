from pathlib import Path
import sys,tempfile
from private_guest import private_guest
from vga_smoke import LocalConsole
qemu,iso,ram=sys.argv[1:4]
with tempfile.TemporaryDirectory(prefix='nx-vga-control-') as temp:
 with private_guest(qemu,iso,ram,'vga-control',{'/tmp/vgactrl':'vga-control.elf'},extra_args=['-vga','std','-qmp',f'unix:{temp}/qmp,server=on,wait=off']) as guest:
  screen=LocalConsole(guest,temp)
  try:
   guest.expect(r'root@nexix.localdomain# ')
   guest.command('/tmp/vgactrl',stdout='VGA_CONTROLS_OK\n')
   text=screen.expect('   ZQ');assert 'old text' not in text
   raw=screen.path.read_bytes();index=(2*80+3)*2
   assert raw[index:index+4]==b'Z\x70Q\x07'
   assert all(raw[i]==32 for i in range(0,4000,2) if i not in [index,index+2])
  finally:screen.close()
print(f'PASS: {ram} real VGA cursor, clearing, standout and fragmented device writes')
