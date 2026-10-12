"""Validate README roadmap IDs, status totals and explicit inventory coverage."""
from collections import Counter
import re


ROW = re.compile(r'^\| (\d+) \| (.*?) \| (Done|In progress|To do) \| (.*?) \|$')
TOTALS = re.compile(r'\*\*(\d+) tracking tasks: (\d+) done, (\d+) in progress\s+and (\d+) to do\*\*')


def validate_roadmap(manifest, coverage, readme):
    """Pure checker; mappings describe planned coverage, not completed ports."""
    errors = []

    def need(condition, message):
        if not condition:
            errors.append(message)

    if not isinstance(coverage, dict):
        return ['roadmap coverage must be an object']
    need(coverage.get('schema_version') == 2, 'unsupported roadmap coverage schema')
    need(coverage.get('version') == manifest.get('version') == '0.1.0', 'roadmap version drift')
    need(coverage.get('roadmap') == 'README.md', 'unexpected roadmap path')
    need(coverage.get('inventory') == 'docs/v7-userland-manifest.json', 'unexpected roadmap inventory path')
    need(isinstance(coverage.get('policy'), str) and bool(coverage['policy']), 'missing roadmap coverage policy')
    count = coverage.get('task_count')
    if type(count) is not int or count < 1:
        errors.append('invalid roadmap task count')
        return errors
    if not isinstance(readme, str):
        return errors + ['roadmap README must be text']
    rows = []
    for line in readme.splitlines():
        if re.match(r'^\|\s*\d+\s*\|', line):
            match = ROW.fullmatch(line)
            if not match:
                errors.append('malformed roadmap row: ' + line)
                continue
            identifier, description, status, notes = match.groups()
            identifier = int(identifier)
            rows.append((identifier, description, status, notes))
            need(bool(description.strip()) and bool(notes.strip()), f'roadmap task {identifier}: missing description or notes')
            if status == 'Done':
                # Self-referencing a commit hash inside that commit is impossible.
                # Such rows explicitly point to Git history; other rows pin hashes.
                need(bool(re.search(r'`[0-9a-f]{7,40}`', notes)) or 'Git history' in notes,
                     f'roadmap task {identifier}: completed task needs commit evidence')
    identifiers = [row[0] for row in rows]
    need(len(rows) == count and identifiers == list(range(1, len(rows) + 1)),
         'roadmap IDs must be unique, ordered and contiguous through task_count')
    states = {row[0]: row[2] for row in rows}
    totals = TOTALS.search(readme)
    need(totals is not None, 'missing roadmap status summary')
    if totals:
        total, done, progressing, todo = map(int, totals.groups())
        actual = Counter(row[2] for row in rows)
        need(total == count and (done, progressing, todo) ==
             (actual['Done'], actual['In progress'], actual['To do']), 'roadmap status summary drift')
    referenced = set()
    for field, source in [('components', 'entries'), ('requirements', 'requirements')]:
        mapping = coverage.get(field)
        if not isinstance(mapping, dict):
            errors.append(f'roadmap {field} mapping must be an object')
            continue
        records = {record.get('id'): record for record in manifest.get(source, []) if isinstance(record, dict)}
        for identifier in sorted(set(records) - set(mapping)):
            errors.append(f'unmapped roadmap {field}: {identifier}')
        for identifier in sorted(set(mapping) - set(records)):
            errors.append(f'unknown roadmap {field}: {identifier}')
        for identifier, tasks in mapping.items():
            if not isinstance(tasks, list) or not tasks:
                errors.append(f'{identifier}: roadmap tasks must be a nonempty list')
                continue
            if not all(type(task) is int and 1 <= task <= count for task in tasks):
                errors.append(f'{identifier}: invalid roadmap task reference')
                continue
            need(tasks == sorted(set(tasks)), f'{identifier}: roadmap references must be sorted and unique')
            for task in tasks:
                need(task in states, f'{identifier}: unknown roadmap task {task}')
            referenced.update(tasks)
            if field == 'components' and identifier in records:
                status = records[identifier].get('status')
                if status == 'implemented':
                    need(any(states.get(task) == 'Done' for task in tasks),
                         f'{identifier}: implemented component needs a completed roadmap task')
                else:
                    need(any(states.get(task) in {'To do', 'In progress'} for task in tasks),
                         f'{identifier}: unfinished component cannot map only to completed tasks')
    for task in sorted(set(states) - referenced):
        errors.append(f'unowned roadmap task: {task}')
    return errors
