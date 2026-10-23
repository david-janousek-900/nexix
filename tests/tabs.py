"""tabs: exact control sequences for each named terminal, -n and absent-terminal detection."""
from v7_command import Command
c=Command('tabs',flags=['-Itests/v7-host'])
def stops(n): return (b' '*8+b'\0331')*n
c.check(['300'],b'\0332'+b'\177'*8+b'\r\n'+stops(14)+b'\n')
c.check(['dasi300s'],b'\0332'+b'\177'*8+b'\r\n'+stops(14)+b'\n')
c.check(['-n','300'],b'\0332'+b'\177'*8+b'\r\n'+stops(15)+b'\n')
c.check(['450'],b'\0332'+b'\177'*8+b'\r\n'+b'\b'*16+b' '*8+b'\0339\n'+stops(15)+b'\0330\n')
c.check(['-n','450'],b'\0332'+b'\177'*8+b'\r\n'+b'\b'*16+b'\0339\n'+stops(16)+b'\0330\n')
c.check(['37'],b'\017\0332'+b'\177'*40+b'\r\n'+b'\b'*8+stops(8)+b'\n')
c.check(['tn300'],b'\0332'+b'\177'*8+b'\r\n'+b' '*8+b'\0331'+stops(13)+b'\n')
c.check(['hp'],b'\0333\r'+stops(9)+b'\n')
c.check([],b'')
c.check(['unknown'],b'')
print('PASS: tabs ASan/UBSan control sequences for every terminal type, -n and no-terminal detection')
