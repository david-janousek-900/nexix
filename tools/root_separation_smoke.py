"""Boot fixed RAM recovery and an independently expanded IDE installation."""
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import tempfile
import time
from root_separation_fixture import fixture


def boot(qemu,iso,ram,disk,second,log):
    extra=[] if disk is None else ['-drive',f'file={disk},format=raw,if=ide,index=0']
    process=subprocess.Popen([qemu,'-machine','pc','-accel','tcg','-cpu','qemu64','-smp','1','-m',ram,
        '-cdrom',iso,'-boot','d','-display','none','-serial','stdio','-monitor','none','-no-reboot',*extra],
        stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
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
        marker=b'ROOT RAM' if disk is None else b'ROOT IDE'
        expect(rb'(?s)'+marker+rb'.*BOOT OK\r\nroot@[A-Za-z0-9.-]+# ')
        if disk is None:
            command('ls -1 /usr','adm\n')
            command('echo RAM_RECOVERY_OK','RAM_RECOVERY_OK\n')
        else:
            command('cat /usr/full-only','FULL_ONLY_DATA\n')
            command('cat /etc/rc','# FULL_IDE_CONFIG\n')
            command('/usr/echo FULL_DISK_EXEC','FULL_DISK_EXEC\n')
            if second:command('cat /tmp/saved','FULL_ONLY_DATA\n')
            else:command('cp /usr/full-only /tmp/saved','')
    finally:
        process.terminate()
        try:remaining,_=process.communicate(timeout=3)
        except subprocess.TimeoutExpired:process.kill();remaining,_=process.communicate()
        log.extend(output+remaining)


def main():
    qemu,iso,ram=sys.argv[1:4];log=bytearray();path=Path('build')/f'root-separation-{ram}.log'
    try:
        with tempfile.TemporaryDirectory(prefix='nexix-root-separation-') as temp:
            disk=fixture(temp)
            boot(qemu,iso,ram,None,False,log)
            boot(qemu,iso,ram,disk,False,log);boot(qemu,iso,ram,disk,True,log)
    except (OSError,RuntimeError,AssertionError,subprocess.CalledProcessError) as error:
        path.write_bytes(log);sys.exit(f'FAIL: root separation: {error}; see {path}')
    path.write_bytes(log)
    print(f'PASS: {ram} fixed RAM bootstrap, expanded IDE root/config/ELF, cold-boot persistence and unchanged kernel')


if __name__=='__main__':main()
