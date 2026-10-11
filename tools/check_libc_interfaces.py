"""Use GCC declarations to check that public function names have native exports."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]


def validate(declarations, exports, deferred):
    errors = []
    for name in sorted(declarations - exports - set(deferred)):
        errors.append('declared function has no native export or owner: ' + name)
    for name in sorted(set(deferred) - declarations):
        errors.append('deferred function is not declared: ' + name)
    for name in sorted(set(deferred) & exports):
        errors.append('deferred function now has an export; update disposition: ' + name)
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cc', default='gcc')
    parser.add_argument('--nm', default='nm')
    args = parser.parse_args()
    contract = json.loads((ROOT / 'docs/libc-declared-interfaces.json').read_text())
    if (contract.get('schema_version') != 1 or not isinstance(contract.get('policy'), str)
            or not contract['policy'] or not isinstance(contract.get('deferred'), dict)):
        raise SystemExit('FAIL: unsupported or incomplete interface contract')
    deferred = contract['deferred']
    roadmap = (ROOT / 'README.md').read_text()
    unfinished = {int(n) for n in re.findall(r'^\| (\d+) \| .*? \| (?:To do|In progress) \|', roadmap, re.M)}
    if not all(type(owner) is int and owner in unfinished for owner in deferred.values()):
        raise SystemExit('FAIL: deferred interface needs an unfinished roadmap owner')
    compiler_headers = subprocess.check_output([args.cc, '-print-file-name=include'], text=True).strip()
    headers = sorted(p.relative_to(ROOT / 'libc/include') for p in (ROOT / 'libc/include').rglob('*.h'))
    with tempfile.TemporaryDirectory(prefix='nexix-libc-interfaces-') as folder:
        source = Path(folder) / 'headers.c'
        auxiliary = Path(folder) / 'headers.aux'
        source.write_text(''.join(f'#include <{p}>\n' for p in headers))
        subprocess.run([args.cc, '-std=c11', '-ffreestanding', '-fno-builtin', '-nostdinc',
                        '-isystem', compiler_headers, '-I'+str(ROOT / 'libc/include'),
                        '-I'+str(ROOT / 'include'), '-aux-info', str(auxiliary),
                        '-fsyntax-only', str(source)], check=True)
        declarations = set()
        for line in auxiliary.read_text().splitlines():
            # GCC emits one declaration per line, including grouped prototypes.
            if str(ROOT / 'libc/include') not in line or ' extern ' not in line:
                continue
            match = re.search(r'\*/ extern .*?\b(\w+)\s*\(', line)
            if not match:
                raise SystemExit('FAIL: cannot parse compiler declaration: '+line)
            declarations.add(match[1])
    if not declarations:
        raise SystemExit('FAIL: compiler produced no public declarations')
    output = subprocess.check_output([args.nm, '-g', '--defined-only',
                                      str(ROOT / 'build/libc.a'), str(ROOT / 'build/libm.a')], text=True)
    exports = {line.split()[-1] for line in output.splitlines() if len(line.split()) == 3}
    errors = validate(declarations, exports, deferred)
    for name in ('errno', 'environ', 'stdin', 'stdout', 'stderr', '_ctype_', 'sys_errlist', 'sys_nerr'):
        if name not in exports:
            errors.append('missing exported public data: '+name)
    if errors:
        raise SystemExit('\n'.join('FAIL: '+e for e in errors))
    # Ensure disappearance of required exports and stale exceptions are caught.
    assert validate({'tell'}, set(), {})
    assert validate({'tell'}, {'tell'}, {'tell': 40})
    assert validate(set(), set(), {'tell': 40})
    print(f'PASS: {len(declarations)} public function declarations: '
          f'{len(declarations - set(deferred))} native exports, {len(deferred)} explicitly deferred; public data exports')


if __name__ == '__main__':
    main()
