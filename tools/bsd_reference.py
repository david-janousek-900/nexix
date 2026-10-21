"""Verify/import the pinned 4.4BSD-Lite2 networking reference; never execute it."""
import argparse
import hashlib
import json
import re
from pathlib import Path, PurePosixPath
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[1]
COMMIT = '50587b00e922225c62f1706266587f435898126d'
ARCHIVE_URL = f'https://codeload.github.com/sergev/4.4BSD-Lite2/tar.gz/{COMMIT}'
ARCHIVE_SHA256 = '5a851582c4c3a5053ef2f4f41df347996969a95c3c7236b0b9e99319ff02c3ef'
PREFIX = f'4.4BSD-Lite2-{COMMIT}/'
REFERENCE = 'reference/bsd-lite2'
LICENSE_URL = 'https://www.freebsd.org/copyright/license/'
LICENSE_SHA256 = 'aa202f7c044300bbceb792f4cd0080002c8ee3dabe0fb7bd562c13bf13f9b23f'
SELECTED = (
    'COPYRIGHT',
    'usr/src/sys/i386/include/cpu.h',
    'usr/src/sys/i386/include/param.h',
    'usr/src/sys/i386/include/types.h',
    'usr/src/sys/kern/uipc_domain.c',
    'usr/src/sys/kern/uipc_mbuf.c',
    'usr/src/sys/kern/uipc_proto.c',
    'usr/src/sys/kern/uipc_socket.c',
    'usr/src/sys/kern/uipc_socket2.c',
    'usr/src/sys/kern/uipc_syscalls.c',
    'usr/src/sys/net/if.c',
    'usr/src/sys/net/if.h',
    'usr/src/sys/net/if_arp.h',
    'usr/src/sys/net/if_dl.h',
    'usr/src/sys/net/if_ethersubr.c',
    'usr/src/sys/net/if_llc.h',
    'usr/src/sys/net/if_loop.c',
    'usr/src/sys/net/if_types.h',
    'usr/src/sys/net/netisr.h',
    'usr/src/sys/net/radix.c',
    'usr/src/sys/net/radix.h',
    'usr/src/sys/net/raw_cb.c',
    'usr/src/sys/net/raw_cb.h',
    'usr/src/sys/net/raw_usrreq.c',
    'usr/src/sys/net/route.c',
    'usr/src/sys/net/route.h',
    'usr/src/sys/net/rtsock.c',
    'usr/src/sys/netinet/icmp_var.h',
    'usr/src/sys/netinet/if_ether.c',
    'usr/src/sys/netinet/if_ether.h',
    'usr/src/sys/netinet/igmp.c',
    'usr/src/sys/netinet/igmp.h',
    'usr/src/sys/netinet/igmp_var.h',
    'usr/src/sys/netinet/in.c',
    'usr/src/sys/netinet/in.h',
    'usr/src/sys/netinet/in_cksum.c',
    'usr/src/sys/netinet/in_pcb.c',
    'usr/src/sys/netinet/in_pcb.h',
    'usr/src/sys/netinet/in_proto.c',
    'usr/src/sys/netinet/in_systm.h',
    'usr/src/sys/netinet/in_var.h',
    'usr/src/sys/netinet/ip.h',
    'usr/src/sys/netinet/ip_icmp.c',
    'usr/src/sys/netinet/ip_icmp.h',
    'usr/src/sys/netinet/ip_input.c',
    'usr/src/sys/netinet/ip_mroute.c',
    'usr/src/sys/netinet/ip_mroute.h',
    'usr/src/sys/netinet/ip_output.c',
    'usr/src/sys/netinet/ip_var.h',
    'usr/src/sys/netinet/raw_ip.c',
    'usr/src/sys/netinet/tcp.h',
    'usr/src/sys/netinet/tcp_debug.c',
    'usr/src/sys/netinet/tcp_debug.h',
    'usr/src/sys/netinet/tcp_fsm.h',
    'usr/src/sys/netinet/tcp_input.c',
    'usr/src/sys/netinet/tcp_output.c',
    'usr/src/sys/netinet/tcp_seq.h',
    'usr/src/sys/netinet/tcp_subr.c',
    'usr/src/sys/netinet/tcp_timer.c',
    'usr/src/sys/netinet/tcp_timer.h',
    'usr/src/sys/netinet/tcp_usrreq.c',
    'usr/src/sys/netinet/tcp_var.h',
    'usr/src/sys/netinet/tcpip.h',
    'usr/src/sys/netinet/udp.h',
    'usr/src/sys/netinet/udp_usrreq.c',
    'usr/src/sys/netinet/udp_var.h',
    'usr/src/sys/sys/cdefs.h',
    'usr/src/sys/sys/domain.h',
    'usr/src/sys/sys/errno.h',
    'usr/src/sys/sys/fcntl.h',
    'usr/src/sys/sys/file.h',
    'usr/src/sys/sys/filedesc.h',
    'usr/src/sys/sys/filio.h',
    'usr/src/sys/sys/ioccom.h',
    'usr/src/sys/sys/ioctl.h',
    'usr/src/sys/sys/kernel.h',
    'usr/src/sys/sys/lock.h',
    'usr/src/sys/sys/malloc.h',
    'usr/src/sys/sys/map.h',
    'usr/src/sys/sys/mbuf.h',
    'usr/src/sys/sys/param.h',
    'usr/src/sys/sys/proc.h',
    'usr/src/sys/sys/protosw.h',
    'usr/src/sys/sys/queue.h',
    'usr/src/sys/sys/resource.h',
    'usr/src/sys/sys/resourcevar.h',
    'usr/src/sys/sys/select.h',
    'usr/src/sys/sys/signal.h',
    'usr/src/sys/sys/signalvar.h',
    'usr/src/sys/sys/socket.h',
    'usr/src/sys/sys/socketvar.h',
    'usr/src/sys/sys/sockio.h',
    'usr/src/sys/sys/sysctl.h',
    'usr/src/sys/sys/syslimits.h',
    'usr/src/sys/sys/syslog.h',
    'usr/src/sys/sys/systm.h',
    'usr/src/sys/sys/time.h',
    'usr/src/sys/sys/types.h',
    'usr/src/sys/sys/ucred.h',
    'usr/src/sys/sys/uio.h',
    'usr/src/sys/sys/un.h',
    'usr/src/sys/sys/unistd.h',
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def safe_name(name):
    path = PurePosixPath(name)
    return bool(name) and not path.is_absolute() and '..' not in path.parts and str(path) == name


def archive_index(archive):
    if digest(archive.read_bytes()) != ARCHIVE_SHA256:
        raise ValueError('archive SHA-256 does not match pinned BSD snapshot')
    contents, records = {}, []
    with tarfile.open(archive, 'r:gz') as tar:
        seen = set()
        for member in tar:
            if not safe_name(member.name) or member.name in seen:
                raise ValueError(f'unsafe or duplicate archive path: {member.name}')
            seen.add(member.name)
            if not member.name.startswith(PREFIX):
                continue
            name = member.name[len(PREFIX):]
            if name not in SELECTED:
                continue
            if not member.isfile():
                raise ValueError(f'non-regular selected archive member: {name}')
            data = tar.extractfile(member).read()
            contents[name] = data
            records.append(dict(path=name, size=len(data), sha256=digest(data),
                                mode=member.mode, type='file'))
    if set(contents) != set(SELECTED):
        raise ValueError('archive is missing selected BSD reference files')
    return dict(schema_version=1, release='4.4BSD-Lite2',
                repository='https://github.com/sergev/4.4BSD-Lite2', commit=COMMIT,
                archive_url=ARCHIVE_URL, archive_sha256=ARCHIVE_SHA256,
                archive_prefix=PREFIX, reference_root=f'{REFERENCE}/source',
                license_document=dict(path='berkeley-license.html', url=LICENSE_URL,
                                      sha256=LICENSE_SHA256),
                files=sorted(records, key=lambda record: record['path'])), contents


def regular_path(path, base):
    """Reject symbolic links in the file or any ancestor below the checkout."""
    return not any(p.is_symlink() for p in (path, *path.parents) if p != base and base in p.parents)


def dependency_audit(root=ROOT):
    """Lexical include ledger, including conditional branches (not build closure)."""
    base = root / REFERENCE / 'source'
    result = []
    for name in SELECTED:
        if name == 'COPYRIGHT':
            continue
        path = PurePosixPath(name)
        text = (base / name).read_text(encoding='latin1')
        includes = []
        for include in sorted(set(re.findall(r'^\s*#\s*include\s*[<"]([^>"\n]+)[>"]', text, re.M))):
            target = PurePosixPath('usr/src/sys') / include
            if include.startswith('machine/'):
                target = PurePosixPath('usr/src/sys/i386/include') / include[8:]
            elif '/' not in include:
                target = path.parent / include
            retained = str(target) in SELECTED
            deferred = include.startswith(('netns/', 'netiso/', 'netccitt/')) or include in {
                'bpfilter.h', 'net/bpf.h', 'hy.h', 'imp.h', 'sys/ktrace.h', 'sys/ioctl_compat.h'}
            includes.append(dict(include=include, disposition='reference' if retained else
                                 'deferred-feature' if deferred else 'native-adapter',
                                 reference=str(target) if retained else None))
        part = path.parts[3]
        tasks = {'net': [418, 419, 427], 'netinet': [419, 420, 421, 422, 423],
                 'kern': [413, 414, 415, 424, 425, 426],
                 'sys': [413, 414, 424, 425], 'i386': [413]}[part]
        result.append(dict(path=name, tasks=tasks, license='Berkeley notice with 1999 amendment',
                           includes=includes))
    return dict(schema_version=1, scope='Lexical includes, including disabled feature branches; not a compilable dependency closure.',
                files=result)


def check_reference(index, root=ROOT):
    errors = []
    expected_metadata = dict(schema_version=1, release='4.4BSD-Lite2',
                             repository='https://github.com/sergev/4.4BSD-Lite2',
                             commit=COMMIT, archive_url=ARCHIVE_URL,
                             archive_sha256=ARCHIVE_SHA256, archive_prefix=PREFIX,
                             reference_root=f'{REFERENCE}/source',
                             license_document=dict(path='berkeley-license.html',
                                                   url=LICENSE_URL, sha256=LICENSE_SHA256))
    if not isinstance(index, dict) or any(index.get(k) != v for k, v in expected_metadata.items()):
        return ['invalid BSD reference provenance']
    records = index.get('files')
    if not isinstance(records, list):
        return ['invalid BSD reference file list']
    names = []
    base = root / REFERENCE / 'source'
    for record in records:
        if not isinstance(record, dict):
            errors.append('invalid BSD reference record')
            continue
        name = record.get('path')
        if not isinstance(name, str) or not safe_name(name) or name not in SELECTED or name in names:
            errors.append(f'unsafe, duplicate or unselected BSD reference path: {name}')
            continue
        names.append(name)
        path = base / name
        if not regular_path(path, root) or not path.is_file():
            errors.append(f'missing/non-regular BSD reference file: {name}')
            continue
        data = path.read_bytes()
        if record.get('size') != len(data) or record.get('sha256') != digest(data):
            errors.append(f'BSD reference content mismatch: {name}')
        if record.get('type') != 'file' or type(record.get('mode')) is not int:
            errors.append(f'invalid BSD reference metadata: {name}')
    if names != list(SELECTED):
        errors.append('BSD reference selection/order mismatch')
    actual = {p.relative_to(base).as_posix() for p in base.rglob('*') if p.is_file() or p.is_symlink()}
    if actual != set(SELECTED):
        errors.append('BSD reference contains missing or unrecorded files')
    license_path = root / REFERENCE / 'berkeley-license.html'
    if (not regular_path(license_path, root) or not license_path.is_file() or
            digest(license_path.read_bytes()) != LICENSE_SHA256):
        errors.append('Berkeley license amendment document mismatch')
    license_copy = root / 'LICENSES/BSD-4.4-Lite2.txt'
    original = base / 'COPYRIGHT'
    if not license_copy.is_file() or not original.is_file() or license_copy.read_bytes() != original.read_bytes():
        errors.append('BSD distribution license copy mismatch')
    return errors


def import_reference(index, contents, root=ROOT):
    """Preflight the whole selection; never overwrite differing reference bytes."""
    base = root / REFERENCE
    writes = {base / 'source' / name: data for name, data in contents.items()}
    writes[root / 'LICENSES/BSD-4.4-Lite2.txt'] = contents['COPYRIGHT']
    writes[base / 'provenance.json'] = (json.dumps(index, indent=2) + '\n').encode()
    for path, data in writes.items():
        if not regular_path(path, root) or (path.exists() and (not path.is_file() or path.read_bytes() != data)):
            raise ValueError(f'refusing to overwrite differing/non-regular reference: {path}')
    for path, data in writes.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists():
            path.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', nargs='?', type=Path)
    parser.add_argument('--import-reference', action='store_true')
    parser.add_argument('--write-audit', action='store_true', help='write the derived include/ownership ledger')
    args = parser.parse_args()
    if args.import_reference and args.archive is None:
        parser.error('--import-reference requires the pinned archive')
    ledger = ROOT / REFERENCE / 'provenance.json'
    if args.archive:
        index, contents = archive_index(args.archive)
        if args.import_reference:
            import_reference(index, contents)
        elif json.loads(ledger.read_text()) != index:
            raise ValueError('committed provenance differs from pinned archive index')
    errors = check_reference(json.loads(ledger.read_text()))
    if errors:
        raise ValueError('\n'.join(errors))
    audit_path = ROOT / REFERENCE / 'audit.json'
    audit = dependency_audit()
    if args.write_audit:
        audit_path.write_text(json.dumps(audit, indent=2) + '\n')
    elif json.loads(audit_path.read_text()) != audit:
        raise ValueError('BSD dependency audit differs from retained sources')
    print(f'PASS: {len(SELECTED)} pinned BSD reference files and Berkeley license document')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, tarfile.TarError) as error:
        sys.exit(f'FAIL: {error}')
