"""Keep the reference review totals and requirement prose tied to canonical data."""
from collections import Counter
import json
from pathlib import Path
import re
from check_roadmap import ROW

ROOT = Path(__file__).resolve().parents[1]


def requirement_table(manifest):
    return ('| Requirement | State | Supplied inputs / remaining work |\n|---|---|---|\n' +
            '\n'.join(f"| `{r['id']}` | {r['status']} | {r['description']} |"
                      for r in manifest['requirements']))


def validate(manifest, coverage, roadmap, review, inventory):
    errors = []
    states = Counter(match[3] for line in roadmap.splitlines() if (match := ROW.fullmatch(line)))
    files = len({source for entry in manifest['entries'] for source in entry['sources']})
    expected = (f"Reference audit totals: **{files:,} files; {len(manifest['entries'])} components; "
                f"{len(manifest['requirements'])} requirements; {coverage['task_count']} tasks "
                f"({states['Done']} done, {states['In progress']} in progress, {states['To do']} to do).**")
    summaries = [line for line in review.splitlines() if line.startswith('Reference audit totals:')]
    if summaries != [expected]:
        errors.append('reference review totals differ from canonical inventory/roadmap')
    start, end = '<!-- requirements:start -->', '<!-- requirements:end -->'
    if inventory.count(start) != 1 or inventory.count(end) != 1:
        errors.append('requirement table needs exactly one marker pair')
    else:
        match = re.search(re.escape(start) + r'\n(.*?)\n' + re.escape(end), inventory, re.S)
        if not match or match[1] != requirement_table(manifest):
            errors.append('documented requirement table differs from canonical inventory')
    return errors


def main():
    read = lambda name: (ROOT / 'docs' / name).read_text()
    errors = validate(json.loads(read('v7-userland-manifest.json')),
                      json.loads(read('roadmap-coverage.json')), read('tasks.md'),
                      read('reference-review.md'), read('porting-inventory.md'))
    if errors:
        raise SystemExit('\n'.join('FAIL: ' + error for error in errors))
    print('PASS: reference review totals and supporting-requirement documentation')


if __name__ == '__main__':
    main()
