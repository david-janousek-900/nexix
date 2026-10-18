"""Manifest-driven QEMU acceptance on disposable staged IDE roots."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import tempfile
import time
from install_manifest import INVENTORY, MANIFEST, guest_path
from mk_rootfs import build
from mk_v7fs import write_image


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_suite(path):
    suite=json.loads(Path(path).read_text())
    if suite.get('schema_version')!=1 or suite.get('terminals',1) not in (1,2):raise ValueError('invalid suite version/terminals')
    if type(suite.get('single_user',False)) is not bool:raise ValueError('invalid single_user fixture flag')
    owners={e['id'] for e in json.loads(INVENTORY.read_text())['entries']}
    seen=set()
    for case in suite['cases']:
        if not re.fullmatch('[A-Za-z0-9_-]+',case['id']) or case['id'] in seen or case['component'] not in owners:raise ValueError('invalid case identity/component')
        seen.add(case['id'])
        if not case['steps']:raise ValueError('empty case')
        for step in case['steps']:
            if type(step.get('terminal',0)) is not int or not 0<=step.get('terminal',0)<suite.get('terminals',1):raise ValueError('invalid terminal')
            if 'command' in step:
                if not isinstance(step['command'],str) or '\n' in step['command'] or len(step['command'])>180:raise ValueError('invalid command')
                if type(step.get('status',0)) is not int or not 0<=step.get('status',0)<=255:raise ValueError('invalid exit status')
            elif 'expect' not in step:raise ValueError('step requires command or expect')
            for key in ('expect','prompt'):
                if key in step:re.compile(step[key])
    return suite


class Guest:
    def __init__(self,qemu,iso,disk,ram,terminals=1,timeout=20,rtc='2024-02-29T12:34:56'):
        self.temp=tempfile.TemporaryDirectory(prefix='nx-serial-');self.sockets=[];self.writers=[];self.buffers=[bytearray() for _ in range(terminals)];self.positions=[0]*terminals
        self.timeout=timeout;self.process=None;self.diagnostics=tempfile.TemporaryFile()
        paths=[str(Path(self.temp.name)/str(i)) for i in range(terminals)]
        args=[qemu,'-machine','pc','-accel','tcg','-cpu','qemu64','-smp','1','-m',ram,'-rtc',f'base={rtc},clock=vm','-cdrom',str(iso),'-boot','d','-display','none','-monitor','none','-no-reboot']
        if disk is not None:args+=['-drive',f'file={disk},format=raw,if=ide,index=0']
        for path in paths:
            os.mkfifo(path+'.in');os.mkfifo(path+'.out')
            self.sockets.append(os.open(path+'.out',os.O_RDWR|os.O_NONBLOCK))
            self.writers.append(os.open(path+'.in',os.O_RDWR|os.O_NONBLOCK))
            args+=['-serial','pipe:'+path]
        try:
            self.process=subprocess.Popen(args,stdout=self.diagnostics,stderr=subprocess.STDOUT)
        except BaseException:self.close();raise
    def pump(self,timeout=.1):
        for peer in select.select(self.sockets,[],[],timeout)[0]:
            data=os.read(peer,65536)
            if not data:raise RuntimeError('guest serial closed')
            out=self.buffers[self.sockets.index(peer)];out.extend(data)
            if len(out)>2*1024*1024:raise RuntimeError('serial output limit')
        if self.process.poll() is not None:raise RuntimeError('QEMU exited')
    def expect(self,pattern,terminal=0,timeout=None):
        pattern=re.compile(pattern.encode() if isinstance(pattern,str) else pattern)
        deadline=time.monotonic()+(self.timeout if timeout is None else timeout)
        while time.monotonic()<deadline:
            out=self.buffers[terminal];start=self.positions[terminal];match=pattern.search(out,start)
            if match:
                before=bytes(out[start:match.start()]);self.positions[terminal]=match.end();return before,match
            if b'PANIC' in out:raise RuntimeError('kernel panic')
            self.pump()
        raise RuntimeError(f'terminal {terminal} timed out waiting for {pattern.pattern!r}')
    def send(self,text,terminal=0):
        data=text.encode() if isinstance(text,str) else text
        while data:
            try:data=data[os.write(self.writers[terminal],data):]
            except BlockingIOError:self.pump()
    def command(self,line,terminal=0,stdout='',status=0,prompt=r'[#$] '):
        token=f'__NX_STATUS_{time.monotonic_ns():x}_'
        wire=line+'; echo '+token+'$?__'
        self.send(wire+'\n',terminal)
        self.expect(re.escape(wire.encode()+b'\r\n'),terminal)
        output,match=self.expect(rb'(?m)^'+token.encode()+rb'([0-9]+)__\r?\n',terminal)
        self.expect(prompt,terminal)
        actual=output.replace(b'\r\n',b'\n').decode('utf-8',errors='replace')
        if actual!=stdout or int(match[1])!=status:raise AssertionError(f'{line!r}: output={actual!r}, status={int(match[1])}; expected={stdout!r}/{status}')
    def close(self):
        if self.process is not None:
            self.process.terminate()
            try:self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:self.process.kill();self.process.wait()
        for peer in self.sockets+self.writers:os.close(peer)
        self.sockets=[];self.diagnostics.close();self.temp.cleanup()


def run_suite(suite_path,report,qemu='qemu-system-x86_64',iso='build/nexix.iso',ram='128M',stage='build/stage',manifest=MANIFEST,components=None):
    suite=load_suite(suite_path);cases=[c for c in suite['cases'] if not components or c['component'] in components]
    if not cases:raise ValueError('no selected cases')
    report=Path(report);report.parent.mkdir(parents=True,exist_ok=True)
    results={'schema_version':1,'suite_sha256':sha(suite_path),'manifest_sha256':sha(manifest),'kernel_iso_sha256':sha(iso),'ram':ram,'cases':[]}
    overrides={'/etc/ttys':b''} if suite.get('single_user') else None
    results['fixture_overrides']={path:hashlib.sha256(data).hexdigest() for path,data in (overrides or {}).items()}
    try:
        with tempfile.TemporaryDirectory(prefix='nx-acceptance-') as temp:
            disk=Path(temp)/'root.img';data=build(None,None,None,manifest,stage,spare_inodes=256,spare_blocks=8192,fixture_overrides=overrides);write_image(disk,data)
            if hasattr(data,'close'):data.close()
            guest=Guest(qemu,iso,disk,ram,suite.get('terminals',1))
            try:
                guest.expect(suite.get('startup',r'(?s)NEXIX_BOOT_OK\r\n# '))
                for case in cases:
                    record={'id':case['id'],'component':case['component'],'status':'failed'};results['cases'].append(record)
                    try:
                        for step in case['steps']:
                            terminal=step.get('terminal',0)
                            if 'command' in step:guest.command(step['command'],terminal,step.get('stdout',''),step.get('status',0),step.get('prompt',r'[#$] '))
                            else:
                                if 'send' in step:guest.send(step['send'],terminal)
                                guest.expect(step['expect'],terminal,step.get('timeout'))
                        record['status']='passed'
                    except BaseException as error:record['error']=str(error);raise
            finally:
                for i,out in enumerate(guest.buffers):report.with_suffix(f'.tty{i}.log').write_bytes(out)
                guest.diagnostics.seek(0);report.with_suffix(".qemu.log").write_bytes(guest.diagnostics.read())
                guest.close()
        results['status']='passed'
    except BaseException as error:results['status']='failed';results['error']=str(error);raise
    finally:report.write_text(json.dumps(results,indent=2)+'\n')
    return results


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('suite',type=Path);parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--qemu',default='qemu-system-x86_64');parser.add_argument('--iso',default='build/nexix.iso');parser.add_argument('--ram',default='128M')
    parser.add_argument('--stage',default='build/stage');parser.add_argument('--manifest',type=Path,default=MANIFEST);parser.add_argument('--component',action='append')
    args=parser.parse_args()
    try:result=run_suite(args.suite,args.report,args.qemu,args.iso,args.ram,args.stage,args.manifest,args.component)
    except (OSError,ValueError,RuntimeError,AssertionError) as error:parser.exit(1,f'FAIL: guest acceptance: {error}\n')
    print(f'PASS: {len(result["cases"])} component cases ({args.ram}); {args.report}')


if __name__=='__main__':main()
