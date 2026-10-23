"""Offline, backed-up native V7 image upgrades from verified installation staging."""
import argparse
from collections import Counter, defaultdict
from dataclasses import dataclass, field
import fcntl
import hashlib
import json
import mmap
import os
from pathlib import Path, PurePosixPath
import stat
import struct
import tempfile
import time
from install_manifest import MANIFEST, load_retired, read_stage
from mk_v7fs import Image, BSIZE, DIR, REG, CHR, BLK, MAX_INODE_BLOCKS, MAX_FILE_BLOCKS, file_blocks, long, short, write_image


def u16(data, offset): return struct.unpack_from('<H',data,offset)[0]
def u32(data, offset): return (u16(data,offset)<<16)|u16(data,offset+2)
def digest(data): return hashlib.sha256(data).hexdigest()


@dataclass(eq=False)
class Node:
    ino: int
    mode: int
    uid: int
    gid: int
    times: tuple
    size: int = 0
    device: int = 0
    extents: dict = field(default_factory=dict)
    payload: bytes | None = None
    links: int = 1

    @property
    def kind(self): return self.mode & 0o170000


class Volume:
    """Validate the complete reachable namespace, ownership and V7 free chains."""
    def __init__(self,data):
        self.data=data
        if len(data)<3*BSIZE: raise ValueError('truncated V7 image')
        self.isize=u16(data,512);self.blocks=u32(data,514)
        if not 3<=self.isize<self.blocks or len(data)!=self.blocks*BSIZE:
            raise ValueError('not a whole native V7 volume')
        self.capacity=(self.isize-2)*8
        if self.capacity>65535: raise ValueError('inode area exceeds V7 limits')
        self.nodes={};self.used=set();self.paths={};self.children={}
        for ino in range(1,self.capacity+1):
            offset=1024+(ino-1)*64;mode=u16(data,offset)
            if not mode: continue
            if ino==1: raise ValueError('allocated bad-block inode is not supported')
            kind=mode&0o170000;nlink=u16(data,offset+2);size=u32(data,offset+8)
            if kind not in (DIR,REG,CHR,BLK) or not 1<=nlink<=32767 or size>MAX_FILE_BLOCKS*BSIZE:
                raise ValueError(f'invalid inode {ino}')
            node=Node(ino,mode,u16(data,offset+4),u16(data,offset+6),
                      tuple(u32(data,offset+n) for n in (52,56,60)),size,links=nlink)
            roots=[(data[offset+12+i*3]<<16)|data[offset+13+i*3]|(data[offset+14+i*3]<<8) for i in range(13)]
            if kind in (CHR,BLK):
                if size or roots[0]>65535 or any(roots[1:]):raise ValueError('invalid device inode')
                node.device=roots[0]
            else:
                for logical,block in enumerate(roots[:10]):
                    if block:self.claim_data(node,logical,block)
                first=10
                for depth,block in enumerate(roots[10:],1):
                    if block:self.indirect(node,block,depth,first)
                    first+=128**depth
            self.nodes[ino]=node
        if 2 not in self.nodes or self.nodes[2].kind!=DIR:raise ValueError('missing root directory')
        pending=[('/',2,2)];seen_dirs=set();references=Counter()
        while pending:
            path,ino,parent=pending.pop()
            node=self.nodes.get(ino)
            if node is None:raise ValueError('directory references an unallocated inode')
            self.paths[path]=node;references[ino]+=1
            if node.kind!=DIR:continue
            if ino in seen_dirs:raise ValueError('directory cycle or hard link')
            seen_dirs.add(ino)
            if node.size%16 or node.size>16*65535:raise ValueError('invalid directory size')
            children={}
            content=self.contents(node)
            for offset in range(0,node.size,16):
                child=u16(content,offset)
                if not child:continue
                raw=content[offset+2:offset+16];name=raw.split(b'\0',1)[0]
                name=name.decode('utf-8','surrogateescape')
                if not name or '/' in name or name in children:raise ValueError('invalid or duplicate directory name')
                children[name]=child
            if children.pop('.',None)!=ino or children.pop('..',None)!=parent:
                raise ValueError('invalid directory parent links')
            self.children[ino]=children
            for name,child in reversed(list(children.items())):
                pending.append((path.rstrip('/')+'/'+name,child,ino))
        if set(references)!=set(self.nodes):raise ValueError('unreachable allocated inode; recover filesystem first')
        for ino,node in self.nodes.items():
            expected=2+sum(self.nodes[n].kind==DIR for n in self.children[ino].values()) if node.kind==DIR else references[ino]
            if node.links!=expected:raise ValueError('inode link-count mismatch')
        self.validate_free()

    def claim(self,block):
        if not self.isize<=block<self.blocks or block in self.used:
            raise ValueError('invalid, duplicate or overlapping data block')
        self.used.add(block)

    def claim_data(self,node,logical,block):
        if logical*BSIZE>=node.size:raise ValueError('data pointer past EOF')
        self.claim(block);node.extents[logical]=block

    def indirect(self,node,block,depth,first):
        self.claim(block)
        for i in range(128):
            pointer=u32(self.data,block*BSIZE+i*4)
            if not pointer:continue
            logical=first+i*128**(depth-1)
            if logical*BSIZE>=node.size:raise ValueError('indirect pointer past EOF')
            if depth==1:self.claim_data(node,logical,pointer)
            else:self.indirect(node,pointer,depth-1,logical)

    def validate_free(self):
        n=u16(self.data,518)
        if not 1<=n<=50:raise ValueError('invalid free-block stack')
        stack=[u32(self.data,520+4*i) for i in range(n)];free=set()
        while stack:
            block=stack.pop()
            if not block:
                if stack:raise ValueError('premature free-chain sentinel')
                break
            if not self.isize<=block<self.blocks or block in free or block in self.used:
                raise ValueError('corrupt free chain or allocated/free overlap')
            free.add(block)
            if not stack:
                n=u16(self.data,block*BSIZE)
                if not 1<=n<=50:raise ValueError('invalid free-chain count')
                stack=[u32(self.data,block*BSIZE+2+4*i) for i in range(n)]
        if len(free)!=u32(self.data,930) or len(free)+len(self.used)!=self.blocks-self.isize:
            raise ValueError('free-block accounting mismatch')
        if u16(self.data,934)!=self.capacity-1-len(self.nodes):raise ValueError('free-inode accounting mismatch')
        n=u16(self.data,720)
        if n>100:raise ValueError('invalid cached free-inode count')
        cached=[u16(self.data,722+2*i) for i in range(n)]
        if len(set(cached))!=len(cached) or any(not 2<=i<=self.capacity or i in self.nodes for i in cached):
            raise ValueError('invalid cached free inode')

    def chunks(self,node):
        for logical,block in sorted(node.extents.items()):
            yield logical,self.data[block*BSIZE:block*BSIZE+min(BSIZE,node.size-logical*BSIZE)]

    def contents(self,node):
        if node.payload is not None:return node.payload
        data=bytearray(node.size)
        for logical,payload in self.chunks(node):data[logical*BSIZE:logical*BSIZE+len(payload)]=payload
        return bytes(data)

    def hash(self,node):
        if node.payload is not None:return digest(node.payload)
        h=hashlib.sha256();position=0;zeros=bytes(65536)
        for logical,payload in self.chunks(node):
            gap=logical*BSIZE-position
            while gap:n=min(gap,len(zeros));h.update(zeros[:n]);gap-=n
            h.update(payload);position=logical*BSIZE+len(payload)
        gap=node.size-position
        while gap:n=min(gap,len(zeros));h.update(zeros[:n]);gap-=n
        return h.hexdigest()


def plan(volume,records,payloads,epoch,retired=()):
    paths=dict(volume.paths);changes=[]
    kinds={'directory':DIR,'file':REG,'character':CHR,'block':BLK}
    for entry in records:
        path=entry['path'];old=paths.get(path);kind=entry['type']
        if kind=='link':
            target=paths[entry['target']]
            if old is not target:
                if old and old.kind!=target.kind:raise ValueError('type conflict: '+path)
                paths[path]=target;changes.append(('link',path))
            continue
        if old and old.kind!=kinds[kind]:raise ValueError('type conflict: '+path)
        # Configuration, accounts, session databases and all existing directories
        # retain contents and metadata. Installed scripts are managed executables.
        managed=kind=='file' and (entry.get('source',{}).get('root')=='programs' or int(entry['mode'],8)&0o111)
        if old and not managed:
            if kind in ('character','block') and old.device!=entry['device']:
                raise ValueError('device-number conflict: '+path)
            continue
        mode=kinds[kind]|int(entry['mode'],8);payload=payloads.get(path,b'')
        if old and (old.mode,old.uid,old.gid)==(mode,entry['uid'],entry['gid']) and volume.hash(old)==digest(payload):continue
        paths[path]=Node(0,mode,entry['uid'],entry['gid'],(epoch,epoch,epoch),
                         len(payload),entry.get('device',0),payload=payload if kind=='file' else None)
        changes.append(('replace' if old else 'add',path))
    # Retired installations are removed only while they are still regular
    # files; anything an administrator put there instead is kept.
    for r in retired:
        node=paths.get(r['path'])
        if node is not None and node.kind==REG:
            del paths[r['path']];changes.append(('remove',r['path']))
    return paths,changes


def retirement_notes(volume,paths,retired):
    """Kept retired paths, and kept configuration that still names one."""
    kept=[r['path'] for r in retired if r['path'] in paths]
    warnings=[]
    names=[(r['path'].encode(),r['replacement']) for r in retired]
    for path,node in sorted(paths.items()):
        if node.kind!=REG or node.payload is not None or node.size>65536:continue
        if not (path.startswith('/etc/') or PurePosixPath(path).name=='.profile'):continue
        text=volume.contents(node)
        for old,new in names:
            at=text.find(old)
            while at>=0 and at+len(old)<len(text) and (chr(text[at+len(old)]).isalnum() or text[at+len(old)] in b'._'):
                at=text.find(old,at+1)
            if at>=0:warnings.append(f'{path} still names retired {old.decode()}; use {new}')
    profile=paths.get('/root/.profile')
    if retired and profile is not None and profile.kind==REG and profile.payload is None:
        for line in volume.contents(profile).splitlines():
            if line.startswith(b'PATH=') and b'/sbin' not in line:
                warnings.append('/root/.profile PATH lacks /sbin and /usr/sbin, where system commands now live')
    return kept,warnings


def build_upgrade(volume,paths,epoch):
    nodes=set(paths.values());occupied={n.ino for n in nodes if n.ino};available=(i for i in range(3,65536) if i not in occupied)
    for node in paths.values():
        if not node.ino:node.ino=next(available)
    needed=max(n.ino for n in nodes)
    inode_blocks=max(volume.isize-2,(needed+2048+7)//8)
    inode_blocks=min(MAX_INODE_BLOCKS,inode_blocks)
    if needed>inode_blocks*8:raise ValueError('V7 inode capacity exhausted')
    children=defaultdict(list);parents={paths['/']:2};refs=Counter(paths.values())
    for path,node in paths.items():
        if path=='/':continue
        parent=paths[str(PurePosixPath(path).parent)]
        if parent.kind!=DIR:raise ValueError('non-directory parent: '+path)
        children[parent].append((PurePosixPath(path).name,node))
        if node.kind==DIR:parents[node]=parent.ino
    # Retain at least the old capacity; grow for new payloads and a free-space budget.
    blocks=2+inode_blocks
    for node in nodes:
        if node.kind==REG:
            # Upper bound permits sparse images without allocating their holes.
            blocks+=file_blocks(node.size) if node.payload is not None else min(file_blocks(node.size),len(node.extents)*4)
        elif node.kind==DIR:blocks+=file_blocks(16*(2+len(children[node])))
    blocks=max(volume.blocks,blocks+32768)
    image=Image(blocks,inode_blocks)
    for node in nodes:
        if node.kind==DIR:continue
        image.inode(node.ino,node.mode,device=node.device,nlink=refs[node],uid=node.uid,gid=node.gid)
        if node.kind==REG:
            roots=[0]*13
            chunks=((i//BSIZE,node.payload[i:i+BSIZE]) for i in range(0,node.size,BSIZE)) if node.payload is not None else volume.chunks(node)
            for logical,payload in chunks:
                block=image.pointer(roots,logical);image.sector(block)[:len(payload)]=payload
            offset=1024+(node.ino-1)*64
            from mk_v7fs import address
            image.bytes[offset+8:offset+12]=long(node.size)
            image.bytes[offset+12:offset+51]=b''.join(address(x) for x in roots)
    # Directory link counts require child directory modes to be present first.
    for node in nodes:
        if node.kind==DIR:image.inode(node.ino,node.mode,uid=node.uid,gid=node.gid)
    for node in nodes:
        if node.kind==DIR:
            entries=[(name,child.ino) for name,child in children[node]]
            raw=bytearray()
            for name,ino in [('.',node.ino),('..',parents[node])]+entries:
                encoded=name.encode('utf-8','surrogateescape')
                if not 1<=len(encoded)<=14 or b'/' in encoded or b'\0' in encoded:raise ValueError('invalid V7 name')
                raw+=short(ino)+encoded.ljust(14,b'\0')
            image.inode(node.ino,node.mode,raw,nlink=2+sum(child.kind==DIR for _,child in children[node]),uid=node.uid,gid=node.gid)
    for node in nodes:
        times=node.times
        if node.kind==DIR and node.ino in volume.children:
            before=volume.children[node.ino]
            after={name:child.ino for name,child in children[node]}
            if before!=after:times=(times[0],epoch,epoch)
        offset=1024+(node.ino-1)*64+52
        image.bytes[offset:offset+12]=b''.join(long(t) for t in times)
    data=image.finish()
    # Keep filesystem labels, interleave and superblock time in their V7 encoding.
    data[936:952]=volume.data[936:952]
    data[926:930]=long(epoch)
    return data


def sync_directory(path):
    fd=os.open(path,os.O_RDONLY|os.O_DIRECTORY)
    try:os.fsync(fd)
    finally:os.close(fd)


def backup(source,path):
    path=Path(path)
    if os.path.lexists(path):raise ValueError('backup already exists')
    name=None
    try:
        with tempfile.NamedTemporaryFile(prefix='.nexix-backup-',dir=path.parent,delete=False) as out:
            name=out.name
            for offset in range(0,len(source),1024*1024):out.write(source[offset:offset+1024*1024])
            out.flush();os.fsync(out.fileno())
        os.link(name,path);sync_directory(path.parent)
    finally:
        if name is not None:os.unlink(name)


def upgrade(image_path,stage,manifest=MANIFEST,backup_path=None,apply=False,epoch=None):
    image_path=Path(image_path);epoch=int(time.time()) if epoch is None else epoch
    if not 0<=epoch<=0xffffffff:raise ValueError('upgrade time exceeds V7 timestamp range')
    records,payloads=read_stage(stage,manifest)
    retired=load_retired(manifest) if manifest is not None else []
    fd=os.open(image_path,os.O_RDWR|os.O_NOFOLLOW)
    candidate=None;result=None
    try:
        info=os.fstat(fd)
        if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or info.st_uid!=os.geteuid():raise ValueError('image must be a regular file with one host link')
        fcntl.lockf(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        with mmap.mmap(fd,0,access=mmap.ACCESS_READ) as original:
            volume=Volume(original);paths,changes=plan(volume,records,payloads,epoch,retired)
            kept,warnings=retirement_notes(volume,paths,retired)
            report={'image':str(image_path),'changes':[{'action':a,'path':p} for a,p in changes],
                    'preserved_configuration':[e['path'] for e in records if e['path'] in volume.paths and e['type']=='file' and e.get('source',{}).get('root')=='config' and not int(e['mode'],8)&0o111],
                    'retired_kept':kept,'warnings':warnings,'applied':False}
            if not apply or not changes:return report
            if backup_path is None:raise ValueError('--backup is required for --apply')
            old_hash=digest(original);backup(original,backup_path)
            result=build_upgrade(volume,paths,epoch)
            verified=Volume(result)
            if set(verified.paths)!=set(paths):raise ValueError('candidate namespace verification failed')
            for path,node in paths.items():
                actual=verified.paths[path]
                if (actual.ino,actual.mode,actual.uid,actual.gid,actual.device)!=(node.ino,node.mode,node.uid,node.gid,node.device):
                    raise ValueError('candidate metadata verification failed: '+path)
                if node.kind!=DIR and actual.times!=node.times:raise ValueError('candidate timestamps changed: '+path)
                if node.kind==REG and verified.hash(actual)!=volume.hash(node):raise ValueError('candidate payload verification failed: '+path)
            with tempfile.NamedTemporaryFile(prefix='.nexix-upgrade-',dir=image_path.parent,delete=False) as out:
                candidate=out.name
                fcntl.lockf(out.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
                for offset in range(0,len(result),1024*1024):out.write(result[offset:offset+1024*1024])
                os.fchown(out.fileno(),-1,info.st_gid);os.fchmod(out.fileno(),stat.S_IMODE(info.st_mode))
                out.flush();os.fsync(out.fileno())
                current=os.stat(image_path,follow_symlinks=False)
                if (current.st_dev,current.st_ino)!=(info.st_dev,info.st_ino) or os.fstat(fd).st_size!=info.st_size or os.fstat(fd).st_mtime_ns!=info.st_mtime_ns or digest(original)!=old_hash:
                    raise ValueError('working image changed during upgrade')
                os.replace(candidate,image_path);candidate=None;sync_directory(image_path.parent)
            report.update(applied=True,backup=str(backup_path),backup_sha256=old_hash)
            return report
    finally:
        if candidate is not None:os.unlink(candidate)
        if hasattr(result,'close'):result.close()
        os.close(fd)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image',type=Path,required=True)
    parser.add_argument('--stage',type=Path,default=Path('build/stage'))
    parser.add_argument('--manifest',type=Path,default=MANIFEST)
    parser.add_argument('--backup',type=Path)
    parser.add_argument('--apply',action='store_true',help='replace the offline image; default only previews changes')
    args=parser.parse_args()
    try:report=upgrade(args.image,args.stage,args.manifest,args.backup,args.apply)
    except (OSError,ValueError,KeyError,StopIteration) as error:parser.exit(1,f'FAIL: disk upgrade: {error}\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
