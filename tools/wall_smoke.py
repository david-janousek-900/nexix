"""Check V7 wall broadcasts, permissions, input bounds and malformed records."""
import re
import struct
import sys
from private_guest import private_guest
from install_manifest import ROOT


def main():
    qemu,iso,ram=sys.argv[1:4]
    for name,data in [('wall-max.bin',b'M'*2999+b'\n'),('wall-big.bin',b'X'*3001),
        ('wall-bad.bin',struct.pack('<8s8sq',b'notty',b'bad',1709210096)),('wall-notty.bin',b'PRESERVED\n')]:
        (ROOT/'build'/name).write_bytes(data)
    artifacts={'/tmp/max-message':'wall-max.bin','/tmp/big-message':'wall-big.bin',
        '/tmp/bad-utmp':'wall-bad.bin','/dev/notty':'wall-notty.bin'}
    with private_guest(qemu,iso,ram,'wall',artifacts,2,single_user=False) as guest:
        for line,name in enumerate(('alice','bob')):
            guest.expect('login: ',line);guest.send(name+'\n',line);guest.expect('Password:',line);guest.send(name+'\n',line);guest.expect(re.escape('$ '),line)
        header=b'\x07\x07\x07Broadcast Message ...\r\n\r\n'
        def broadcast(command,body,peers=(0,1),prompt='$ ',error=None):
            start=len(guest.buffers[0]);wire=command+'; echo WALL_STATUS_$?'
            guest.send(wire+'\n');guest.expect(re.escape(wire+'\r\n'))
            for peer in peers:guest.expect(re.escape(header+body.replace(b'\n',b'\r\n')),peer)
            guest.expect('WALL_STATUS_0\r\n'+re.escape(prompt))
            if error:
                guest.positions[0]=start;guest.expect(re.escape(error+'\r\n'))
        guest.command('echo WALL_TEXT > /tmp/message',prompt=re.escape('$ '))
        broadcast('/etc/wall /tmp/message',b'WALL_TEXT\n')
        broadcast('echo PIPE_TEXT | /etc/wall',b'PIPE_TEXT\n')
        guest.command('mesg n',1,status=1,prompt=re.escape('$ '))
        before=len(guest.buffers[1]);broadcast('/etc/wall /tmp/message',b'WALL_TEXT\n',(0,),error='cannot open /dev/ttyS1')
        if b'WALL_TEXT' in guest.buffers[1][before:]:raise AssertionError('non-root bypassed mesg n')
        guest.send('su\n');guest.expect('Password:');guest.send('nexix\n');guest.expect('# ')
        broadcast('/etc/wall /tmp/message',b'WALL_TEXT\n',prompt='# ')
        broadcast('/etc/wall /tmp/max-message',b'M'*2999+b'\n',prompt='# ')
        guest.command('/etc/wall /tmp/big-message',stdout='wall: message too long\n',status=1)
        guest.command('/etc/wall /missing',stdout='Cannot open /missing\n',status=1)
        guest.command('cp /tmp/bad-utmp /etc/utmp')
        broadcast('/etc/wall /tmp/message',b'',(),prompt='# ',error='cannot open /dev/notty')
        guest.command('cat /dev/notty',stdout='PRESERVED\n')
        guest.command('echo WALL_PARENT_ALIVE',stdout='WALL_PARENT_ALIVE\n')
        guest.command('echo WALL_PEER_ALIVE',1,'WALL_PEER_ALIVE\n',prompt=re.escape('$ '))
    print(f'PASS: {ram} wall file/stdin broadcasts, ordinary/root permissions, 3000-byte boundary, oversize/missing input and safe malformed-record device checks')


if __name__=='__main__':main()
