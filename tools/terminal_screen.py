"""Small ANSI/VT screen observer for serial acceptance (not a guest emulator).

Only emitted controls in the native termcap subset are accepted. Delayed margin
wrap and non-destructive tabs follow ANSI terminal behavior; VGA is observed
independently through QMP's physical framebuffer.
"""
class TerminalScreen:
    def __init__(self,rows=24,cols=80):
        self.height,self.width=rows,cols
        self.cells=[[' ']*cols for _ in range(rows)]
        self.row=self.col=0;self.pending=False;self.escape=None
    def newline(self):
        self.row+=1
        if self.row==self.height:self.cells.pop(0);self.cells.append([' ']*self.width);self.row-=1
    def feed(self,data):
        for c in data:
            if self.escape is not None:
                if self.escape=='':
                    if c!=91:raise AssertionError(f'unknown serial ESC {c}')
                    self.escape='[';continue
                if 64<=c<=126:
                    raw=self.escape[1:];self.escape=None
                    params=[int(x or '0') for x in raw.split(';')]
                    n=params[0] or 1;op=chr(c)
                    if op in 'Hf':self.row=min(self.height-1,n-1);self.col=min(self.width-1,(params[1] or 1)-1 if len(params)>1 else 0)
                    elif op=='A':self.row=max(0,self.row-n)
                    elif op=='B':self.row=min(self.height-1,self.row+n)
                    elif op=='C':self.col=min(self.width-1,self.col+n)
                    elif op=='D':self.col=max(0,self.col-n)
                    elif op in 'JK':
                        at=self.row*self.width+self.col
                        start,end=(0,self.height*self.width-1) if op=='J' else (self.row*self.width,(self.row+1)*self.width-1)
                        if params[0]==0:start=at
                        elif params[0]==1:end=at
                        elif params[0]!=2:raise AssertionError('unknown erase mode')
                        for i in range(start,end+1):self.cells[i//self.width][i%self.width]=' '
                    elif op!='m':raise AssertionError(f'unknown serial CSI {raw}{op}')
                    if op!='m':self.pending=False
                else:self.escape+=chr(c)
                continue
            if c==27:self.escape='';continue
            if c==13:self.col=0;self.pending=False
            elif c==10:self.newline();self.pending=False
            elif c==8:self.col=max(0,self.col-1);self.pending=False
            elif c==9:self.col=min(self.width-1,(self.col|7)+1);self.pending=False
            elif c>=32 and c!=127:
                if self.pending:self.col=0;self.newline();self.pending=False
                self.cells[self.row][self.col]=chr(c)
                if self.col==self.width-1:self.pending=True
                else:self.col+=1
    def screen(self):return [''.join(row).rstrip() for row in self.cells]
