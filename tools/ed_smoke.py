"""Native V7 editor scripts, interactive signals and independent rotor fixtures."""
import ctypes
from pathlib import Path
import re
import sys
import time
from private_guest import private_guest
from multiuser_acceptance import login


def rotor(payload,key):
    # Independent historical-width reference: system DES crypt plus explicit
    # signed PDP-11 long / unsigned-short arithmetic, not the editor's C code.
    lib=ctypes.CDLL('libcrypt.so.1');lib.crypt.argtypes=[ctypes.c_char_p,ctypes.c_char_p];lib.crypt.restype=ctypes.c_char_p
    encoded=lib.crypt(key[:8],key[:2]);assert encoded and len(encoded)==13
    seed=123
    for i,c in enumerate(encoded):seed=(seed*c+i)&0xffffffff
    first=list(range(256));third=[0]*256
    for i in range(256):
        seed=(5*seed+encoded[i%13])&0xffffffff
        signed=seed if seed<0x80000000 else seed-0x100000000
        random=((abs(signed)%65521)*(-1 if signed<0 else 1))&0xffff
        k=255-i;index=(random&255)%(k+1);random>>=8
        first[k],first[index]=first[index],first[k]
        if third[k]:continue
        assert k
        index=(random&255)%k
        while third[index]:index=(index+1)%k
        third[k]=index;third[index]=k
    second=[0]*256
    for i,c in enumerate(first):second[c]=i
    return bytes((second[(third[(first[(c+(i&255))&255]+((i>>8)&255))&255]-((i>>8)&255))&255]-(i&255))&255 for i,c in enumerate(payload))


def main():
    qemu,iso,ram=sys.argv[1:4]
    artifacts={'/tmp/edprobe':'ed-recover.elf'}
    def fixture(name,data):
        artifact='ed-'+name;Path('build',artifact).write_bytes(data.encode() if isinstance(data,str) else data)
        artifacts['/tmp/'+name]=artifact
    basic='a\nalpha\nbeta\ngamma\n.\n1i\nstart\n.\n2c\nALPHA\n.\n3d\n$s/gamma/GAMMA/\n1,$p\nw /tmp/edited\nq\n'
    fixture('editcmd',basic);fixture('reopen','1,$p\nq\n')
    lines=''.join(f'line{i:03d}\n' for i in range(600))
    fixture('many',lines)
    fixture('growcmd','1,600t0\n1,$s/line/L\\\nX/\nw /tmp/expanded\nq\n')
    fixture('badcmd','a\nx\n.\n999999999999999999999p\nQ\n')
    fixture('badre','a\nx\n.\n1s/\\1/x/\nQ\n')
    fixture('shellcmd','!echo ED_SHELL_OK\nQ\n')
    fixture('regexcmd','a\none\ntwo\nthree\n.\n1ka\n1m3\n1t$\ng/two/s/two/TWO/\nv/TWO/d\n1,2j\ns/TWO/one/\nu\np\nQ\n')
    plaintext=b'alpha\nbeta\n'*80
    fixture('plain',plaintext);fixture('cipher',rotor(plaintext,b'secret'))
    with private_guest(qemu,iso,ram,'ed',artifacts,2,single_user=False) as guest:
        login(guest,'root','nexix')
        guest.command('ed - < /tmp/editcmd',stdout='start\nALPHA\nGAMMA\n')
        guest.command('cat /tmp/edited',stdout='start\nALPHA\nGAMMA\n')
        guest.command('ed - /tmp/edited < /tmp/reopen',stdout='start\nALPHA\nGAMMA\n')
        guest.command('ed - /tmp/many < /tmp/growcmd')
        guest.command('cat /tmp/expanded',stdout=(''.join(f'L\nX{i:03d}\n' for i in range(600)))*2)
        guest.command('ed - < /tmp/shellcmd',stdout='ED_SHELL_OK\n!\n')
        guest.command('ed - < /tmp/regexcmd',stdout='TWOTWO\n')
        guest.command('ed - < /tmp/badcmd',stdout='?\n')
        guest.command('ed - < /tmp/badre',stdout='?\n')
        # Exercise a real terminal and encrypted temporary blocks, not piped keys.
        guest.send('ed - -x /tmp/cipher\n');guest.expect('Key:\r\n')
        def unlock(path):
            # TIOCSETP restores echo and flushes queued input after the key.
            # Repeating the harmless filename command handshakes with that boundary.
            guest.send('secret\n');deadline=time.monotonic()+10
            while path.encode() not in guest.buffers[0][guest.positions[0]:]:
                if time.monotonic()>deadline:raise AssertionError('editor did not finish key setup')
                guest.pump(.05);guest.send('f\n')
            guest.expect(re.escape(path+'\r\n'))
        start=len(guest.buffers[0]);unlock('/tmp/cipher')
        guest.send('1,$p\n');guest.expect(re.escape(plaintext.decode().replace('\n','\r\n')))
        assert b'secret' not in guest.buffers[0][start:]
        guest.send('w /tmp/encoded\n');guest.expect('w /tmp/encoded\r\n')
        guest.send('q\n');guest.expect('# ')
        # Native test shell has no cmp yet: decrypt the saved cipher and verify bytes.
        guest.send('ed - -x /tmp/encoded\n');guest.expect('Key:\r\n');unlock('/tmp/encoded')
        guest.send('1,$p\n');guest.expect(re.escape(plaintext.decode().replace('\n','\r\n')))
        guest.send('Q\n');guest.expect('# ')
        guest.send('ed -\n');guest.expect('ed -\r\n');guest.send('a\nkeep\n.\n');guest.expect('keep\r\n.\r\n');guest.send('1p\n');guest.expect('1p\r\nkeep\r\n')
        guest.send('\x03');guest.expect(re.escape('\r\n?\r\n'))
        guest.send('1p\n');guest.expect('1p\r\nkeep\r\n')
        guest.send('Q\n');guest.expect('# ')
        guest.command('/tmp/edprobe',stdout='ED_RECOVERY_OK\n')
        login(guest,'alice','alice',1,prompt='$ ')
        guest.command('echo Q | ed - /root/.profile',1,stdout='?/root/.profile\n',prompt=re.escape('$ '))
    print(f'PASS: {ram} ed edit/save/reopen, addressing/global/move/copy/undo, reallocations, errors, encrypted files, SIGINT/SIGHUP and permissions')


if __name__=='__main__':main()
