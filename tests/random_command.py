"""V7 random: fixed-clock native libc sequence against an independent Python oracle."""
from v7_command import Command

def expected(data,divisor,exit_mode=False):
    state=12345;result=b''
    def draw():
        nonlocal state
        state=(state*1103515245+12345)&0xffffffff
        return (state>>16)&32767
    if not exit_mode:
        for line in data.splitlines():
            if draw()/32768*divisor<1:result+=line+b'\n'
    status=int(draw()/32768*divisor)&255
    return result,status
if __name__=='__main__':
    c=Command('random',sources=['user/v7/cmd/random.c','libc/numbers.c','tests/random-clock.c'],flags=('-DRANDOM_CLOCK=nx_random_clock',))
    data=b''.join(f'line{i}\n'.encode() for i in range(80))
    for divisor in [0,1,2,3,7,256]:
        out,status=expected(data,divisor)
        for flag in [[],['-r']]:c.check([*flag,divisor],data=data,stdout=out,status=status)
        out,status=expected(b'',divisor,True);c.check(['-e',divisor],stdout=out,status=status)
    c.check([1],data=b'x'*10000+b'\n'+b'a\0b\n',stdout=b'x'*10000+b'\n'+b'a\0b\n')
    c.check([0],data=b'last without newline',stdout=b'last without newline\n')
    for text in ['nan','inf','1e100','not-a-number','2147483647']:
        c.check([text],stdout='',stderr='random: invalid divisor\n',status=1)
    c.check(['-z'],stdout='',stderr='random: invalid option\n',status=1)
    print('PASS: random fixed-clock native PRNG oracle, line/exit modes, zero/all selection, long/NUL lines and invalid bounds (ASan/UBSan)')
