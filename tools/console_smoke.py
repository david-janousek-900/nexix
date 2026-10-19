"""Verify boot, init, shell and panic use only the selected console."""
from pathlib import Path
import shutil
import re
import sys
import tempfile
import time
from guest_acceptance import Guest
from multiuser_acceptance import login
from vga_smoke import LocalConsole


def check(qemu, ram, mode):
    iso='build/nexix.iso' if mode=='vga' else 'build/nexix-serial.iso'
    with tempfile.TemporaryDirectory(prefix='nx-console-') as temp:
        disk=Path(temp)/'root.img'
        shutil.copyfile('build/distribution.img',disk)
        guest=Guest(qemu,iso,disk,ram,2,extra_args=[
            '-vga','std','-qmp',f'unix:{temp}/qmp,server=on,wait=off'])
        screen=None
        try:
            screen=LocalConsole(guest,temp)
            if mode=='vga':
                text=screen.expect('login: ')
                assert 'x86_64 kernel entered' in text, text
                assert 'Nexix 0.1.0' in text, text
                screen.type('root\n');screen.expect('Password:')
                screen.type('nexix\n');screen.expect('# ')
                assert re.search(r'\[ *[0-9]+\.[0-9]{6}\] x86_64 kernel entered',text),text
                screen.type('pwd\n');screen.expect('\n/root\n')
                screen.type('echo BACKSPACE_BAD');screen.type('\x7f'*3)
                text=screen.expect('echo BACKSPACE_')
                assert 'BACKSPACE_BAD' not in text,text
                screen.type('OK\n');screen.expect('\nBACKSPACE_OK\n')
                screen.type('echo SECOND_BAD');screen.type('\x08'*3)
                screen.type('OK\n');screen.expect('\nSECOND_OK\n')
                screen.type('echo VGA_SELECTED\n');screen.expect('\nVGA_SELECTED\n')
                screen.type('echo CONSOLE_DEVICE > /dev/console\n');screen.expect('\nCONSOLE_DEVICE\n')
                screen.type('dmesg\n');screen.expect('] BOOT OK\nroot@nexix.localdomain # ')
                screen.type('\x04');screen.expect('login: ')
                # Neither UART may get boot messages, prompts, echo or shell output.
                guest.send('root\nnexix\necho WRONG_UART\n',0)
                guest.send('root\nnexix\necho WRONG_UART\n',1)
                guest.pump(.2)
                assert not any(guest.buffers), guest.buffers
            else:
                login(guest,'root','nexix')
                assert re.search(rb'\[ *[0-9]+\.[0-9]{6}\] x86_64 kernel entered',guest.buffers[0])
                guest.command('pwd',stdout='/root\n')
                guest.command('echo $HOME',stdout='/root\n')
                guest.send('echo BAD\x7f\x7f\x7fOK\n');guest.expect('\r\nOK\r\nroot@[A-Za-z0-9.-]+ # ')
                guest.send('echo BAD\x08\x08\x08OK\n');guest.expect('\r\nOK\r\nroot@[A-Za-z0-9.-]+ # ')
                assert b'Nexix 0.1.0' in guest.buffers[0]
                guest.command('echo SERIAL_SELECTED',stdout='SERIAL_SELECTED\n')
                guest.command('echo CONSOLE_DEVICE > /dev/console',stdout='CONSOLE_DEVICE\n')
                guest.send('\x04');guest.expect('login: ')
                assert not guest.buffers[1], guest.buffers[1]
                assert 'Nexix' not in screen.screen(), screen.screen()
            # An NMI exercises the production panic path without a test kernel.
            screen.call('inject-nmi')
            if mode=='vga':
                screen.expect('PANIC: unhandled CPU exception')
                guest.pump(.1)
                assert not any(guest.buffers), guest.buffers
            else:
                deadline=time.monotonic()+10
                while b'PANIC: unhandled CPU exception' not in guest.buffers[0]:
                    if time.monotonic()>deadline:
                        raise AssertionError('serial panic diagnostic missing')
                    guest.pump(.05)
                assert 'Nexix' not in screen.screen(), screen.screen()
                assert not guest.buffers[1], guest.buffers[1]
            print(f'PASS: {ram} {mode}: exclusive boot, rc, login, console writes, logout and panic',flush=True)
        finally:
            if screen:
                Path(f'build/console-{mode}-{ram}.screen.txt').write_text(screen.screen())
                screen.close()
            for n,data in enumerate(guest.buffers):
                Path(f'build/console-{mode}-{ram}-serial{n}.log').write_bytes(data)
            guest.close()


if __name__=='__main__':
    for mode in ('vga','serial'):
        check(sys.argv[1],sys.argv[2],mode)
