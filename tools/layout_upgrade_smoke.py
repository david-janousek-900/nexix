"""Upgrade a disk with the V7 /etc layout to the BSD one (task 433), then
cold-boot it before and after.

The old disk is the current distribution with the nine relocated binaries
back in /etc, the pre-432 init (built from Git, so it execs /etc/getty), the
old root profile and an administrator's old-style /etc/netstart."""
from pathlib import Path
import sys
import tempfile
from guest_acceptance import Guest
from install_manifest import load_retired
from multiuser_acceptance import login
from mk_v7fs import REG, write_image
from upgrade_disk import Volume, Node, build_upgrade, upgrade

PROMPT = r'root@[A-Za-z0-9.-]+# '


def old_disk(path, old_init, old_profile, old_netstart):
    source = Volume(Path('build/distribution.img').read_bytes())
    paths = dict(source.paths)
    for r in load_retired():
        paths[r['path']] = paths.pop(r['replacement'])
    for directory in ('/sbin', '/usr/sbin', '/usr/bin'):
        paths.pop(directory)
    def file(name, payload, mode):
        old = paths.get(name)
        paths[name] = Node(old.ino if old else 0, REG | mode, 0, 0, (123, 456, 789), len(payload), payload=payload)
    file('/etc/init', old_init, 0o755)
    file('/root/.profile', old_profile, 0o644)
    file('/etc/netstart', old_netstart, 0o755)
    data = build_upgrade(source, paths, 1700000000)
    write_image(path, data)
    if hasattr(data, 'close'): data.close()


def main():
    qemu, iso, ram, old_init, old_profile, old_netstart = sys.argv[1:7]
    nic = ('-nic', 'user,model=e1000')
    with tempfile.TemporaryDirectory(prefix='nx-layout-') as temp:
        disk = Path(temp) / 'root.img'
        old_disk(disk, Path(old_init).read_bytes(), Path(old_profile).read_bytes(), Path(old_netstart).read_bytes())
        # Before: the kernel falls back to /etc/init and says so; the old
        # init and its /etc/netstart still bring the network up.
        g = Guest(qemu, iso, disk, ram, extra_args=nic)
        try:
            g.expect('ROOT OLD LAYOUT: starting /etc/init; upgrade this disk')
            login(g, 'root', 'nexix')
            g.command('/etc/ifconfig em0 | grep inet', stdout='\tinet 10.0.2.15 netmask 255.255.255.0 broadcast 10.0.2.255\n',
                      prompt=PROMPT)
            g.command('test -d /sbin', status=1, prompt=PROMPT)
        finally:
            g.close()
        report = upgrade(disk, Path('build/stage'), backup_path=Path(temp) / 'before.img', apply=True, epoch=1700000001)
        removed = sorted(c['path'] for c in report['changes'] if c['action'] == 'remove')
        assert report['applied'] and removed == sorted(r['path'] for r in load_retired()), removed
        assert report['retired_kept'] == [], report['retired_kept']
        assert report['warnings'] == ['/etc/netstart still names retired /etc/ifconfig; use /sbin/ifconfig',
                                      '/etc/netstart still names retired /etc/route; use /sbin/route',
                                      '/root/.profile PATH lacks /sbin and /usr/sbin, where system commands now live'], \
            report['warnings']
        upgraded = Volume(disk.read_bytes())
        assert not [p for p, n in upgraded.paths.items() if p.startswith('/etc/') and n.kind == REG and
                    upgraded.contents(n)[:4] == b'\x7fELF'], 'ELF programs left in /etc'
        # After: /sbin/init starts directly, /etc holds no system binaries,
        # and the administrator's files are untouched.
        g = Guest(qemu, iso, disk, ram, extra_args=nic)
        try:
            login(g, 'root', 'nexix')
            assert b'ROOT OLD LAYOUT' not in bytes(g.buffers[0])
            c = lambda line, **e: g.command(line, prompt=PROMPT, **e)
            c('for f in init getty wall dmesg mknod accton ifconfig route update; do test -f /etc/$f && echo $f; done; true')
            c('test -f /sbin/init && test -f /usr/libexec/getty && test -f /usr/bin/wall && test -f /usr/sbin/update')   # V7 test has no -x
            c('grep -c /etc/ifconfig /etc/netstart', stdout='1\n')   # kept as written, hence the warning
            c('grep PATH= /root/.profile', stdout='PATH=/bin\n')
            c('/sbin/ifconfig em0 | grep -c inet', stdout='0\n', status=1)   # the stale netstart could not configure it
            c('/sbin/ping -c 1 127.0.0.1 | grep transmitted', stdout='1 packets transmitted, 1 packets received, 0.0% packet loss\n')
        finally:
            g.close()
    print(f'PASS: {ram} V7-layout disk boots with a warning, upgrades to the BSD layout with removals and warnings, then boots cleanly')


if __name__ == '__main__':
    main()
