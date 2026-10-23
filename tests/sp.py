"""sp: horizontal packing at tab stops; long input lines."""
from v7_command import Command
c=Command('sp')
c.check([],'a\tbb\tccc\n',data='a\nbb\nccc\n')
c.check(['13'],'aaaa\tbbbb\ncccc\n',data='aaaa\nbbbb\ncccc\n')
c.check(['-13'],'aaaa\tbbbb\ncccc\n',data='aaaa\nbbbb\ncccc\n')
c.check(['10'],'aaaa\nbbbb\ncccc\n',data='aaaa\nbbbb\ncccc\n')
c.check([],'\n',data='')
long='x'*1000
c.check([],f'\n{long}\ny\n',data=f'{long}\ny\n')
print('PASS: sp ASan/UBSan packing, widths, empty and 1000-byte lines')
