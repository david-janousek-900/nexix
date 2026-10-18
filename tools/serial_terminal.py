"""Prepare or connect to QEMU's second serial pipe. Ctrl-] disconnects."""
import argparse
import os
from pathlib import Path
import select
import stat
import sys
import termios
import tty


def prepare(prefix):
    for suffix in ('.in','.out'):
        path=Path(str(prefix)+suffix)
        try:os.mkfifo(path,0o600)
        except FileExistsError:
            if not stat.S_ISFIFO(path.lstat().st_mode):raise ValueError(f'{path} is not a FIFO')


def connect(prefix):
    peers=[];saved=None
    try:
        for suffix,mode in (('.out',os.O_RDONLY),('.in',os.O_WRONLY)):
            path=Path(str(prefix)+suffix)
            if not stat.S_ISFIFO(path.lstat().st_mode):raise ValueError(f'{path} is not a FIFO')
            peers.append(os.open(path,mode|os.O_NONBLOCK|os.O_NOFOLLOW))
        if os.isatty(0):saved=termios.tcgetattr(0);tty.setraw(0)
        print('COM2 connected; Ctrl-] disconnects.',file=sys.stderr,flush=True)
        while True:
            for peer in select.select([0,peers[0]],[],[])[0]:
                data=os.read(peer,4096)
                if not data:return
                stop=False
                if peer==0 and b'\x1d' in data:data=data.split(b'\x1d',1)[0];stop=True
                target=peers[1] if peer==0 else 1
                while data:
                    try:data=data[os.write(target,data):]
                    except BlockingIOError:select.select([],[target],[],1)
                if stop:return
    finally:
        if saved is not None:termios.tcsetattr(0,termios.TCSADRAIN,saved)
        for peer in peers:os.close(peer)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare',action='store_true');parser.add_argument('prefix',type=Path)
    args=parser.parse_args()
    try:
        if args.prepare:prepare(args.prefix)
        else:connect(args.prefix)
    except (OSError,ValueError) as error:parser.exit(1,f'COM2: {error}; start make run before connecting.\n')


if __name__=='__main__':main()
