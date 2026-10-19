"""Repeated multi-user command, pipe, filesystem and idle-wakeup acceptance.

Uses a disposable IDE root; never opens the working disk. Increase rounds for
longer intermittent-freeze investigations. Logs survive both success and failure.
"""
import argparse
import time
from private_guest import private_guest
from multiuser_acceptance import login


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('qemu')
    parser.add_argument('iso')
    parser.add_argument('ram')
    parser.add_argument('--rounds',type=int,default=100)
    args=parser.parse_args()
    if args.rounds<1:parser.error('rounds must be positive')
    with private_guest(args.qemu,args.iso,args.ram,'stability',{},terminals=2,single_user=False) as guest:
        login(guest,'root','nexix')
        login(guest,'alice','alice',1,prompt='$ ')
        for round_number in range(args.rounds):
            guest.command('uname -n; hostname -s',stdout='nexix.localdomain\nnexix\n')
            guest.command('echo PIPE_OK | cat | cat',1,stdout='PIPE_OK\n')
            guest.command('ls -l /etc/passwd > /tmp/probe; cat /etc/hostname',stdout='nexix.localdomain\n')
            if round_number%20==0:
                # All foreground sessions can sleep; the next command must
                # wake from the real UART IRQ after the scheduler has idled.
                deadline=time.monotonic()+3
                while time.monotonic()<deadline:guest.pump(.1)
        guest.command('rm /tmp/probe')
    print(f'PASS: {args.ram} {args.rounds} multi-user stability rounds, pipelines, disk I/O and idle wakeups')


if __name__=='__main__':main()
