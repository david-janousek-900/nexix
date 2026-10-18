"""Seed a new working disk atomically; existing paths are never replaced."""
import argparse
import os
from pathlib import Path
import shutil
import tempfile


def seed_disk(seed, output):
    output=Path(output)
    if os.path.lexists(output):return False
    output.parent.mkdir(parents=True,exist_ok=True)
    name=None
    try:
        with Path(seed).open('rb') as source:
            with tempfile.NamedTemporaryFile(prefix='.nexix-disk-',dir=output.parent,delete=False) as target:
                name=target.name
                shutil.copyfileobj(source,target,1024*1024)
                target.flush();os.fsync(target.fileno())
        try:os.link(name,output)
        except FileExistsError:return False
        return True
    finally:
        if name is not None:os.unlink(name)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seed',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    try:created=seed_disk(args.seed,args.output)
    except OSError as error:parser.exit(1,f'FAIL: disk seed: {error}\n')
    print(('Created ' if created else 'Preserved ')+str(args.output))


if __name__=='__main__':main()
