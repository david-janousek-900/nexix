"""Validate and stage the native distribution without host device nodes/chown."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path, PurePosixPath
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'docs/native-install.json'
INVENTORY = ROOT / 'docs/v7-userland-manifest.json'
BOOTSTRAP = ROOT / 'docs/bootstrap-install.json'
BOOTSTRAP_CONFIG = ROOT / 'rootfs/bootstrap'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def guest_path(value):
    if not isinstance(value, str) or not value.startswith('/') or str(PurePosixPath(value)) != value:
        raise ValueError(f'invalid guest path: {value}')
    if any(p in ('.', '..') or not p.isascii() or len(p) > 14 for p in value.split('/')[1:]):
        raise ValueError(f'invalid V7 path: {value}')
    return value


def safe_file(base, relative):
    if not isinstance(relative, str) or not relative or PurePosixPath(relative).is_absolute() or any(p in ('', '.', '..') for p in relative.split('/')) or not re.fullmatch(r'[A-Za-z0-9_./-]+', relative):
        raise ValueError(f'unsafe source path: {relative}')
    result = base / relative
    for part in (result, *result.parents):
        if part.is_symlink():
            raise ValueError(f'symlink in source/stage path: {part}')
    return result


def validate_entries(entries):
    if not isinstance(entries, list):
        raise ValueError('entries must be a list')
    paths = {}
    for e in entries:
        path = guest_path(e['path'])
        if path in paths:
            raise ValueError(f'duplicate destination: {path}')
        kind = e['type']
        if kind == 'link':
            target = paths.get(guest_path(e['target']))
            if not target or target['type'] in ('link', 'directory'):
                raise ValueError(f'invalid or forward hard link: {path}')
        else:
            if kind not in ('file', 'directory', 'character', 'block'):
                raise ValueError(f'unknown entry type: {kind}')
            if not isinstance(e['mode'], str) or len(e['mode']) != 4 or any(c not in '01234567' for c in e['mode']):
                raise ValueError(f'invalid mode: {path}')
            for key in ('uid', 'gid'):
                if type(e[key]) is not int or not 0 <= e[key] <= 65535:
                    raise ValueError(f'invalid {key}: {path}')
            if kind in ('character', 'block') and (type(e['device']) is not int or not 0 <= e['device'] <= 65535):
                raise ValueError(f'invalid device: {path}')
            if kind == 'file':
                if e['format'] not in ('elf', 'data', 'empty'):
                    raise ValueError(f'invalid file format: {path}')
                if e['format'] == 'empty':
                    if 'source' in e:
                        raise ValueError(f'empty file has source: {path}')
                else:
                    source = e['source']
                    if source['root'] not in ('programs', 'config'):
                        raise ValueError(f'invalid source root: {path}')
                    safe_file(Path('.'), source['path'])
        paths[path] = e
    if '/' not in paths or paths['/']['type'] != 'directory':
        raise ValueError('missing root directory')
    for path in paths:
        parent = str(PurePosixPath(path).parent)
        if paths.get(parent, {}).get('type') != 'directory':
            raise ValueError(f'missing directory parent: {path}')
    return entries


def load(path=MANIFEST):
    data = json.loads(Path(path).read_text())
    if data['schema_version'] != 1 or data['version'] != '0.2.0' or data['inventory'] != 'docs/v7-userland-manifest.json':
        raise ValueError('unsupported installation manifest')
    entries = validate_entries(data['entries'])
    owners = {e['id']: e for e in json.loads(INVENTORY.read_text())['entries']}
    for e in entries:
        if e['component'] not in owners:
            raise ValueError(f'unknown component: {e["component"]}')
        if 'bootstrap' in e and (e.get('format') != 'elf' or type(e['bootstrap']) is not bool):
            raise ValueError(f'invalid bootstrap selection: {e["path"]}')
        if owners[e['component']]['kind']=='installed_script':
            helper=e.get('format')=='elf' and any(o['install']==e['path'] and
                o['status']=='implemented' and o['role']=='helper' for o in owners[e['component']]['outputs'])
            if (not helper and (e.get('format')!='data' or e.get('source',{}).get('root')!='config')) or not int(e.get('mode','0'),8)&0o111:
                raise ValueError(f'invalid executable script: {e["path"]}')
            if not any(o['install']==e['path'] and o['status']=='implemented' for o in owners[e['component']]['outputs']):
                raise ValueError(f'unimplemented or mismatched script: {e["path"]}')
        if e.get('format') == 'elf':
            name = PurePosixPath(e['path']).name
            if not re.fullmatch(r'[A-Za-z0-9_-]+', name) or e['source'] != {'root': 'programs', 'path': 'v7-'+name+'.elf'}:
                raise ValueError(f'invalid native program artifact: {e["path"]}')
            outputs = owners[e['component']]['outputs']
            if not any(o['install'] == e['path'] and o['status'] == 'implemented' for o in outputs):
                raise ValueError(f'unimplemented or mismatched program: {e["path"]}')
    return entries


def validate_profiles(full, bootstrap):
    if any('bootstrap' in e for e in full):
        raise ValueError('bootstrap flags belong to the fixed bootstrap manifest')
    selected={e['path']:e for e in bootstrap if e.get('format')=='elf'}
    if not {'/sbin/init','/bin/sh'}<=selected.keys() or any(not e.get('bootstrap') for e in selected.values()):
        raise ValueError('bootstrap must embed its init/shell and selected programs')
    installed={e['path']:e for e in full if e.get('format')=='elf'}
    for path,e in selected.items():
        if path not in installed or installed[path]['source']!=e['source'] or installed[path]['component']!=e['component']:
            raise ValueError(f'bootstrap executable missing from full installation: {path}')


def inputs(entries, programs=Path("build"), config=Path("rootfs")):
    return [str((programs if e['source']['root'] == 'programs' else config) / e['source']['path'])
            for e in entries if 'source' in e]


def materialize(entries, programs, config):
    records = []
    payloads = {}
    for entry in entries:
        e = dict(entry)
        if e['type'] == 'file':
            source = e.get('source')
            data = safe_file(programs if source['root'] == 'programs' else config, source['path']).read_bytes() if source else b''
            if e['format'] == 'elf' and (len(data) < 64 or data[:7] != b'\x7fELF\x02\x01\x01' or data[16:20] != b'\x02\x00\x3e\x00'):
                raise ValueError(f'{e["path"]}: expected native x86_64 ELF64')
            e['sha256'] = digest(data)
            payloads[e['path']] = data
        records.append(e)
    return records, payloads


def read_stage(stage, manifest=MANIFEST):
    stage = Path(stage)
    receipt_path = safe_file(stage, 'receipt.json')
    receipt = json.loads(receipt_path.read_text())
    if receipt['schema_version'] != 1:
        raise ValueError('unsupported stage receipt')
    entries = validate_entries(receipt['entries'])
    if manifest is not None and [{k: v for k, v in e.items() if k != 'sha256'} for e in entries] != load(manifest):
        raise ValueError('stage entries differ from manifest')
    if manifest is not None and (receipt['manifest_sha256'] != digest(Path(manifest).read_bytes()) or
                                 receipt['inventory_sha256'] != digest(INVENTORY.read_bytes())):
        raise ValueError('stale stage provenance')
    payloads = {}
    expected = {'receipt.json', 'root'}
    for e in entries:
        if e['type'] not in ('file', 'directory'):
            continue  # Guest device nodes/links are represented only in metadata.
        relative = 'root' + e['path'] if e['path'] != '/' else 'root'
        expected.add(relative)
        target = safe_file(stage, relative)
        if e['type'] == 'directory':
            if not target.is_dir():
                raise ValueError(f'missing stage directory: {e["path"]}')
        else:
            data = target.read_bytes()
            if digest(data) != e['sha256']:
                raise ValueError(f'modified stage payload: {e["path"]}')
            payloads[e['path']] = data
        if target.stat().st_mode & 0o777 != host_mode(e):
            raise ValueError(f'modified stage mode: {e["path"]}')
    actual = {str(p.relative_to(stage)) for p in stage.rglob('*')}
    if actual != expected or any(p.is_symlink() for p in stage.rglob('*')):
        raise ValueError('unexpected stage paths or symlinks')
    return entries, payloads


def host_mode(entry):
    return (int(entry['mode'], 8) & 0o777) | (0o700 if entry['type'] == 'directory' else 0o600)


def stage_tree(destination, manifest=MANIFEST, programs=Path('build'), config=Path('rootfs')):
    entries, payloads = materialize(load(manifest), programs, config)
    receipt = {'schema_version': 1, 'manifest_sha256': digest(Path(manifest).read_bytes()),
               'inventory_sha256': digest(INVENTORY.read_bytes()), 'entries': entries}
    destination = Path(destination)
    # Reject symlink parents and refuse to replace anything except a verified stage.
    safe_file(destination.parent, destination.name)
    if destination.exists():
        read_stage(destination, manifest=None)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.nexix-stage-', dir=destination.parent) as temp:
        new = Path(temp) / 'new'
        new.mkdir()
        for e in sorted(entries, key=lambda e: len(PurePosixPath(e['path']).parts)):
            target = new / ('root' + e['path'])
            if e['type'] == 'directory':
                target.mkdir(parents=True, exist_ok=True)
            elif e['type'] == 'file':
                target.write_bytes(payloads[e['path']])
            else:
                continue
            target.chmod(host_mode(e))  # Guest ownership/set-ID stays metadata.
        (new / 'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
        old = Path(temp) / 'old'
        if destination.exists():
            os.replace(destination, old)
        try:
            os.replace(new, destination)
        except OSError:
            if old.exists():
                os.replace(old, destination)
            raise
        # TemporaryDirectory removes only the previously verified generated stage.
    return len(payloads)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=MANIFEST)
    parser.add_argument('--commands', action='store_true')
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--inputs', action='store_true')
    parser.add_argument('--embedded', action='store_true')
    parser.add_argument('--catalog', action='store_true')
    parser.add_argument('--programs', type=Path, default=Path('build'))
    parser.add_argument('--config', type=Path, default=Path('rootfs'))
    parser.add_argument('--output', type=Path, default=Path('build/stage'))
    args = parser.parse_args()
    try:
        entries = load(args.manifest)
        if args.verify:
            records, payloads = read_stage(args.output, args.manifest)
            print(f'Verified {len(payloads)} staged files in {args.output}')
        elif args.catalog:
            print('#define NX_INSTALL_PROGRAMS(X) \\')
            embedded = [e for e in entries if e.get('bootstrap')]
            for i, e in enumerate(embedded):
                symbol = '_binary_build_' + re.sub(r'[^A-Za-z0-9_]', '_', e['source']['path'])
                print('    X('+json.dumps(e['path'])+', '+symbol+')'+(' \\' if i+1<len(embedded) else ''))
        elif args.embedded:
            print(' '.join(Path(e['path']).name for e in entries if e.get('bootstrap')))
        elif args.commands:
            print(' '.join(Path(e['path']).name for e in entries if e.get('format') == 'elf'))
        elif args.inputs:
            print(' '.join(inputs(entries,args.programs,args.config)))
        else:
            count = stage_tree(args.output, args.manifest, args.programs, args.config)
            print(f'Staged {count} files and guest metadata in {args.output}')
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f'FAIL: install manifest: {error}\n')


if __name__ == '__main__':
    main()
