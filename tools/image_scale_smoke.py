"""Boot a disposable 64 MiB, 4096-inode IDE root twice through real COM1."""
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import tempfile
import time
from mk_large_image_fixture import fixture


def boot(disk, qemu, iso, ram, second, log):
    process=subprocess.Popen([qemu,'-machine','pc','-accel','tcg','-cpu','qemu64','-smp','1','-m',ram,
        '-cdrom',iso,'-boot','d','-drive',f'file={disk},format=raw,if=ide,index=0',
        '-display','none','-serial','stdio','-monitor','none','-no-reboot'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    output=bytearray();position=0
    def expect(pattern):
        nonlocal position
        deadline=time.monotonic()+25
        while time.monotonic()<deadline:
            found=re.search(pattern,output[position:])
            if found:position+=found.end();return
            if process.poll() is not None or b'PANIC' in output:raise RuntimeError('guest failed')
            ready,_,_=select.select([process.stdout],[],[],.2)
            if ready:output.extend(os.read(process.stdout.fileno(),4096))
        raise RuntimeError('guest timeout')
    def command(line,answer):
        process.stdin.write(line.encode()+b'\n');process.stdin.flush()
        expect(rb'\r\n'+re.escape(answer.encode().replace(b'\n',b'\r\n'))+rb'root@[A-Za-z0-9.-]+# ')
    try:
        expect(rb'(?s)ROOT IDE\r\n(?:\[[^\]\r\n]+\] )?ROOT MOUNTED.*BOOT OK\r\nroot@[A-Za-z0-9.-]+# ')
        command('cat /usr/many/f1599','LARGE_INODE_DATA\n')
        command('cat /usr/alias','LARGE_INODE_DATA\n')
        if second:
            command('cat /usr/many/saved','LARGE_INODE_DATA\n')
            command('rm /usr/many/saved','')
        else:
            command('cp /usr/many/f1599 /usr/many/saved','')
            command('cat /usr/many/saved','LARGE_INODE_DATA\n')
    finally:
        process.terminate()
        try:remaining,_=process.communicate(timeout=3)
        except subprocess.TimeoutExpired:process.kill();remaining,_=process.communicate()
        log.extend(output+remaining)


def main():
    qemu,iso,ram=sys.argv[1:4];log=bytearray()
    path=Path('build')/f'image-scale-{ram}.log'
    try:
        with tempfile.TemporaryDirectory(prefix='nexix-image-scale-') as temp:
            disk=Path(temp)/'root.img';fixture(disk)
            boot(disk,qemu,iso,ram,False,log);boot(disk,qemu,iso,ram,True,log)
    except (OSError,RuntimeError) as error:
        path.write_bytes(log);sys.exit(f'FAIL: image scale: {error}; see {path}')
    path.write_bytes(log)
    print(f'PASS: {ram} 64 MiB/4096-inode IDE root, high inode/block access, write and cold-boot persistence')


if __name__=='__main__':main()
