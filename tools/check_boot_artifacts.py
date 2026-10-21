"""Validate provenance and native role associations, without executing old images."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TARGETS = {'boot': 'build/nexix.iso', **{
    name: 'build/nexix.elf'
    for name in ('unix', 'hphtunix', 'hptmunix', 'rphtunix', 'rptmunix')}}


def validate(root, audit):
    errors = []
    def need(value, message):
        if not value:
            errors.append(message)
    def safe_file(value):
        if not isinstance(value, str):
            return False
        path = Path(value)
        return not path.is_absolute() and '..' not in path.parts and (root / path).is_file()
    need(audit.get('schema_version') == 1, 'unsupported schema')
    need(audit.get('component') == 'distribution.boot-kernels' and
         type(audit.get('task')) is int and audit['task'] == 351, 'invalid owner')
    manifest = json.loads((root / 'docs/v7-userland-manifest.json').read_text())
    entry = next(e for e in manifest['entries'] if e['id'] == 'distribution.boot-kernels')
    index = json.loads((root / 'reference/distribution.json').read_text())
    archived = {'reference/v7/' + f['path']: f for f in index['files']}
    records = audit.get('artifacts', [])
    if not isinstance(records, list) or not all(isinstance(r, dict) for r in records):
        return errors + ['invalid artifact records']
    sources = [r.get('source') for r in records]
    expected = {'reference/v7/' + name for name in TARGETS}
    need(len(sources) == len(expected) and all(isinstance(s, str) for s in sources)
         and set(s for s in sources if isinstance(s, str)) == expected,
         'incomplete or duplicate artifact coverage')
    need(set(entry['sources']) == expected, 'inventory ownership drift')
    for record in records:
        source = record.get('source')
        if not isinstance(source, str) or source not in expected:
            errors.append('unknown artifact source')
            continue
        original = archived[source]
        data = (root / source).read_bytes()
        need(record.get('sha256') == original['sha256'] == hashlib.sha256(data).hexdigest(),
             source + ': hash drift')
        need(type(record.get('size')) is int and record['size'] == original['size'] == len(data),
             source + ': size drift')
        need(record.get('disposition') == 'preserved_reference_only', source + ': false execution claim')
        need(record.get('native_artifact') == TARGETS[Path(source).name], source + ': wrong native association')
        for key in ('native_sources', 'historical_evidence'):
            values = record.get(key)
            need(isinstance(values, list) and bool(values), source + ': missing ' + key)
            for path in values if isinstance(values, list) else []:
                need(safe_file(path), source + ': missing or unsafe evidence')
                need(isinstance(path, str) and path.startswith('reference/') == (key == 'historical_evidence'),
                     source + ': evidence classification drift')
    return errors


if __name__ == '__main__':
    audit = json.loads((ROOT / 'docs/boot-kernel-artifacts.json').read_text())
    errors = validate(ROOT, audit)
    if errors:
        raise SystemExit('\n'.join(errors))
    print('boot/kernel artifact provenance: 6 preserved images, native roles verified')
