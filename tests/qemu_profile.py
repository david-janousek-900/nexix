"""The make QEMU network profile (task 429): NIC and HOSTFWD to -nic arguments."""
import subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
def nic(*assignments):
    r = subprocess.run(['make', '-s', '--no-print-directory', 'print-qemu-nic', *assignments], cwd=ROOT,
                       capture_output=True, text=True)
    return r.returncode, r.stdout.strip(), r.stderr
assert nic() == (0, '-nic user,model=e1000', '')
assert nic('NIC=user')[1] == '-nic user,model=e1000'
assert nic('NIC=none')[1] == '-nic none'
assert nic('HOSTFWD=tcp:8080:7000')[1] == '-nic user,model=e1000,hostfwd=tcp::8080-:7000'
assert nic('HOSTFWD=tcp:8080:7000 udp:5300:53')[1] == \
    '-nic user,model=e1000,hostfwd=tcp::8080-:7000,hostfwd=udp::5300-:53'
for bad, message in [('NIC=e1000', 'NIC must be user or none'), ('NIC=', 'NIC must be user or none'),
                     ('HOSTFWD=tcp:1', 'must be tcp|udp:hostport:guestport'),
                     ('HOSTFWD=icmp:1:2', 'must be tcp|udp:hostport:guestport'),
                     ('HOSTFWD=tcp:1:2:3', 'must be tcp|udp:hostport:guestport')]:
    status, out, err = nic(bad)
    assert status and message in err, (bad, status, err)
status, out, err = nic('NIC=none', 'HOSTFWD=tcp:1:2')
assert status and 'HOSTFWD needs NIC=user' in err
# Every interactive run target carries the profile.
text = (ROOT / 'Makefile').read_text()
assert text.count('-no-reboot $(QEMU_NIC_ARGS) $(QEMU_DEBUG_ARGS)') == 2
print('PASS: NIC=user/none, HOSTFWD forwards, rejected profiles and run-target wiring')

# Resolver profile stays opt-in and uses a V7-safe sample filename (454).
import json
records={e['path']:e for e in json.loads((ROOT/'docs/native-install.json').read_text())['entries']}
assert '/etc/resolv.conf' not in records
assert records['/etc/resolv.qemu']['source']=={'root':'config','path':'etc/resolv.qemu'}
assert 'nameserver 10.0.2.3' in (ROOT/'rootfs/etc/resolv.qemu').read_text()
for name in ('hosts','services','protocols','networks'):
    assert records['/etc/'+name]['source']['root']=='config'
print('PASS: optional resolver profile and preserved database configuration entries')
