"""Check complete source responsibility and original syscall-slot coverage."""
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def validate(root, audit, manifest, roadmap_text):
    errors = []
    def need(condition, message):
        if not condition:
            errors.append(message)
    if not isinstance(audit, dict) or audit.get('schema_version') != 2:
        return ['unsupported kernel audit schema']
    need(audit.get('component') == 'distribution.kernel', 'wrong kernel component')
    need(bool(audit.get('policy')), 'missing audit policy')
    entry = next(e for e in manifest['entries'] if e['id'] == 'distribution.kernel')
    expected = set(entry['sources'])
    actual = {str(p.relative_to(root)) for p in (root / 'reference/v7/usr/sys').rglob('*') if p.is_file()}
    need(expected == actual, 'kernel inventory differs from reference tree')
    tasks = {int(m[0]): m[1] for m in re.findall(r'^\| (\d+) \| .*? \| (Done|In progress|To do) \|', roadmap_text, re.M)}
    seen = []
    identifiers = []
    for group in audit.get('groups', []):
        identifiers.append(group.get('id'))
        need(bool(group.get('id')) and bool(group.get('note')), 'missing group description')
        sources = group.get('sources', [])
        seen.extend(sources)
        need(bool(sources), 'empty source group')
        native = group.get('native', [])
        need(bool(native), 'missing native integration points')
        for path in native:
            need(isinstance(path, str) and not Path(path).is_absolute() and '..' not in Path(path).parts
                 and (root / path).is_file(), f'missing or unsafe native path: {path}')
        owners = group.get('tasks', [])
        need(bool(owners) and all(type(n) is int and n in tasks for n in owners), 'invalid group roadmap owners')
    need(len(identifiers) == len(set(identifiers)), 'duplicate group identifier')
    need(len(seen) == len(set(seen)), 'duplicate kernel source')
    need(set(seen) == expected, 'incomplete or unknown kernel sources')
    reference = (root / 'reference/v7/usr/sys/sys/sysent.c').read_text()
    slots = {int(n): routine for routine, n in re.findall(r'\d+,\s*\d+,\s*(\w+),?\s*/\*\s*(\d+)\s*=', reference)}
    records = audit.get('syscalls', [])
    need([r.get('number') for r in records] == list(range(64)), 'syscall slots must cover 0 through 63 in order')
    for record in records:
        number = record.get('number')
        need(type(number) is int and record.get('routine') == slots.get(number), f'syscall reference drift: {number}')
        status = record.get('status')
        need(status in {'historical_stub', 'pending', 'native_path'}, f'invalid syscall status: {number}')
        need((status == 'historical_stub') == (slots.get(number) in {'nosys', 'nullsys'}), f'historical syscall stub drift: {number}')
        owner = record.get('task')
        need(type(owner) is int and owner in tasks, f'invalid syscall owner: {number}')
        if status == 'pending':
            need(tasks.get(owner) in {'To do', 'In progress'}, f'pending syscall has completed owner: {number}')
        need(bool(record.get('label')), f'missing syscall description: {number}')
        need(record.get('native') in {'kernel/v7/syscall.c', 'kernel/v7/syscall_policy.c', 'libc/time.c'}, f'invalid syscall integration point: {number}')
    return errors


def main():
    audit = json.loads((ROOT / 'docs/kernel-coverage.json').read_text())
    manifest = json.loads((ROOT / 'docs/v7-userland-manifest.json').read_text())
    errors = validate(ROOT, audit, manifest, (ROOT / 'docs/tasks.md').read_text())
    if errors:
        raise SystemExit('\n'.join('FAIL: ' + error for error in errors))
    count = sum(len(g['sources']) for g in audit['groups'])
    print(f'PASS: kernel audit covers {count} reference inputs and 64 original syscall slots')


if __name__ == '__main__':
    main()
