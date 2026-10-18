"""Build disposable release-scale roots; never touch the working disk."""
import argparse
import json
from pathlib import Path
import tempfile
from install_manifest import MANIFEST
from mk_rootfs import build
from mk_v7fs import write_image


def fixture(output):
    with tempfile.TemporaryDirectory(prefix='nexix-large-inputs-') as temp:
        base=Path(temp);config=base/'config';config.mkdir()
        # Preserve ordinary bootstrap configuration while adding synthetic data.
        import shutil
        shutil.copytree('rootfs',config,dirs_exist_ok=True)
        (config/'sample').write_bytes(b'LARGE_INODE_DATA\n')
        with (config/'filler').open('wb') as f:f.truncate(34*1024*1024)
        manifest=json.loads(MANIFEST.read_text())
        common=dict(mode='0644',uid=123,gid=456,component='distribution.config')
        manifest['entries'].append(dict(path='/usr/many',type='directory',**dict(common,mode='0755')))
        manifest['entries'].append(dict(path='/usr/filler',type='file',format='data',source=dict(root='config',path='filler'),**common))
        for i in range(1600):
            manifest['entries'].append(dict(path=f'/usr/many/f{i:04d}',type='file',format='data',source=dict(root='config',path='sample'),**common))
        manifest['entries'].append(dict(path='/usr/alias',type='link',target='/usr/many/f1599',component='distribution.config'))
        plan=base/'install.json';plan.write_text(json.dumps(manifest))
        image=build(131072,Path('build'),config,plan,inode_blocks=512)
        write_image(output,image)
        if hasattr(image,'close'):image.close()


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    fixture(parser.parse_args().output)
