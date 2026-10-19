"""Execute consumers linked exclusively against the staged development tree."""
import sys
from private_guest import private_guest

qemu,iso,ram=sys.argv[1:4]
artifacts={f'/tmp/devkit-{n}':f'devkit-{n}.elf' for n in ['crt0','fcrt0','mcrt0','fmcrt0']}
artifacts.update({'/tmp/devkit-cat':'devkit-cat.elf','/tmp/exec-consumer':'exec-consumer.elf'})
with private_guest(qemu,iso,ram,'development',artifacts) as guest:
    guest.expect(r'(?s)BOOT OK\r\nroot@[A-Za-z0-9.-]+ # ')
    guest.command('cd /tmp')
    for startup in ['crt0','fcrt0','mcrt0','fmcrt0']:
        guest.command(f'DEVKIT_TEST=ok /tmp/devkit-{startup} native',stdout='DEVKIT_MAIN\nDEVKIT_EXIT\n')
        if startup in ['mcrt0','fmcrt0']:
            guest.command('/tmp/devkit-crt0 profile',stdout='DEVKIT_PROFILE_OK\n')
            guest.command('rm mon.out')
    guest.command('echo staged-consumer >/tmp/devkit-input')
    guest.command('/tmp/devkit-cat /tmp/devkit-input',stdout='staged-consumer\n')
    guest.command('/tmp/exec-consumer',stdout='EXECVP_SCRIPT_argument\nEXECVP_SCRIPT_argument\nEXEC_CONSUMER_OK\n')
    guest.command('rm /tmp/devkit-input')
print(f'PASS: {ram} installed headers/archives, four startup variants, V7 cat and execvp script consumers')
