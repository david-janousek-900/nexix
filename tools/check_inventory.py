"""Read-only validation of the V7 userland manifest and reference coverage."""
import argparse
import ast
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = 'reference/v7/usr/src'
KINDS = {'command', 'command_group', 'game', 'library', 'prototype', 'build_script', 'support'}
STATUSES = {'implemented', 'partial', 'not_ported', 'prototype', 'pending_assessment'}
REQUIREMENT_STATUSES = {'supplied', 'partial', 'generated', 'missing', 'pending_assessment'}
ROLES = {'installed', 'optional', 'helper', 'build_helper', 'library', 'runtime_object', 'source_only', 'data'}


def validate(manifest, root=ROOT):
    """Return diagnostics without modifying the manifest, reference or build tree."""
    errors = []
    def need(condition, message):
        if not condition:
            errors.append(message)
    def paths(values, owner, reference=False):
        need(isinstance(values, list), f'{owner}: paths must be a list')
        if not isinstance(values, list):
            return
        for value in values:
            if not isinstance(value, str):
                errors.append(f'{owner}: non-string path')
                continue
            path = Path(value)
            safe = not path.is_absolute() and '..' not in path.parts
            need(safe, f'{owner}: unsafe path {value}')
            if safe:
                need((root / path).exists(), f'{owner}: missing path {value}')
                if reference:
                    need(path.is_relative_to(REFERENCE), f'{owner}: source outside reference {value}')
                    need((root / path).is_file(), f'{owner}: source is not a file {value}')
    if not isinstance(manifest, dict):
        return ['manifest must be an object']
    need(manifest.get('schema_version') == 1, 'unsupported schema version')
    need(manifest.get('reference_root') == REFERENCE, 'unexpected reference root')
    need(manifest.get('version') == '0.1.0', 'unexpected release scope')
    for key in ('scope', 'coverage', 'dependency_policy', 'timestamp_policy'):
        need(isinstance(manifest.get(key), str) and bool(manifest[key]), f'missing {key}')
    need(isinstance(manifest.get('exceptions'), list), 'exceptions must be explicit')
    for key in ('historical_source_archives', 'prebuilt_artifacts'):
        paths(manifest.get(key), key, reference=True)
    priorities = manifest.get('priorities', {})
    need(isinstance(priorities, dict) and set(priorities) == {str(i) for i in range(16)}, 'missing priority definitions')
    entries, requirements = manifest.get('entries'), manifest.get('requirements')
    if not isinstance(entries, list) or not isinstance(requirements, list):
        return errors + ['entries and requirements must be lists']
    ids = set()
    for record in entries + requirements:
        if not isinstance(record, dict):
            errors.append('record must be an object')
            continue
        identifier = record.get('id')
        need(isinstance(identifier, str) and bool(re.fullmatch(r'[A-Za-z0-9_.-]+', identifier or '')), 'invalid identifier')
        need(identifier not in ids, f'duplicate identifier: {identifier}')
        if isinstance(identifier, str):
            ids.add(identifier)
    for requirement in requirements:
        if not isinstance(requirement, dict):
            continue
        owner = requirement.get('id', '?')
        need(requirement.get('status') in REQUIREMENT_STATUSES, f'{owner}: invalid requirement status')
        need(bool(requirement.get('category')) and bool(requirement.get('description')), f'{owner}: incomplete requirement')
        paths(requirement.get('evidence'), owner)
        need(bool(requirement.get('evidence')), f'{owner}: requirement needs evidence')
        locations = requirement.get('locations')
        need(isinstance(locations, list) and all(isinstance(x, str) and x.startswith('/') for x in locations), f'{owner}: invalid target locations')
    owned, installed = set(), set()
    entry_map = {}
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        owner = entry.get('id', '?')
        entry_map[owner] = entry
        need(entry.get('kind') in KINDS, f'{owner}: invalid kind')
        need(entry.get('status') in STATUSES, f'{owner}: invalid status')
        need(type(entry.get('priority')) is int and str(entry['priority']) in priorities, f'{owner}: invalid priority')
        need(bool(entry.get('summary')), f'{owner}: missing summary')
        sources = entry.get('sources', [])
        paths(sources, owner, reference=True)
        need(bool(sources), f'{owner}: no sources')
        if isinstance(sources, list) and all(isinstance(x, str) for x in sources):
            need(len(sources) == len(set(sources)), f'{owner}: duplicate source')
            owned.update(sources)
        for key in ('blockers', 'unresolved', 'acceptance'):
            value = entry.get(key)
            need(isinstance(value, list) and all(isinstance(x, str) and x for x in value), f'{owner}: invalid {key}')
        need(bool(entry.get('acceptance')), f'{owner}: no acceptance checks')
        need(bool(entry.get('blockers')) or entry.get('status') == 'implemented' or entry.get('kind') == 'support', f'{owner}: missing blocker assessment')
        outputs = entry.get('outputs')
        need(isinstance(outputs, list), f'{owner}: outputs must be a list')
        for output in outputs if isinstance(outputs, list) else []:
            if not isinstance(output, dict):
                errors.append(f'{owner}: invalid output')
                continue
            need(bool(output.get('name')), f'{owner}: unnamed output')
            need(output.get('role') in ROLES and output.get('status') in STATUSES, f'{owner}: invalid output classification')
            location = output.get('install')
            need(location is None or isinstance(location, str) and location.startswith('/'), f'{owner}: invalid install location')
            paths(output.get('evidence'), owner)
            if output.get('status') == 'implemented':
                need(bool(output.get('evidence')), f'{owner}: implemented output needs evidence')
                need(entry.get('status') == 'implemented', f'{owner}: output/entry implementation status differs')
                installed.add(output.get('name'))
            if entry.get('status') == 'implemented':
                need(output.get('status') == 'implemented', f'{owner}: implemented component has unimplemented output')
        dependencies = entry.get('dependencies')
        need(isinstance(dependencies, list), f'{owner}: dependencies must be a list')
        for dependency in dependencies if isinstance(dependencies, list) else []:
            if not isinstance(dependency, dict):
                errors.append(f'{owner}: invalid dependency')
                continue
            need(dependency.get('target') in ids, f'{owner}: unknown dependency {dependency.get("target")}')
            need(dependency.get('phase') in {'build', 'runtime', 'optional_runtime'}, f'{owner}: invalid dependency phase')
            need(dependency.get('certainty') in {'confirmed', 'inferred', 'unresolved'}, f'{owner}: invalid dependency certainty')
            need(bool(dependency.get('note')), f'{owner}: dependency needs explanation')
            paths(dependency.get('evidence'), owner)
            if dependency.get('certainty') == 'confirmed':
                need(bool(dependency.get('evidence')), f'{owner}: confirmed dependency needs evidence')
        includes = entry.get('reference_includes')
        need(isinstance(includes, list), f'{owner}: include audit missing')
        for include in includes if isinstance(includes, list) else []:
            if not isinstance(include, dict):
                errors.append(f'{owner}: invalid include')
                continue
            need(bool(include.get('name')), f'{owner}: unnamed include')
            paths(include.get('evidence'), owner, reference=True)
    reference = root / REFERENCE
    actual = {str(p.relative_to(root)) for p in reference.rglob('*') if p.is_file()}
    for path in sorted(actual - owned):
        errors.append(f'unclassified reference file: {path}')
    archives = {path for path in actual if path.endswith('.c.a')}
    need(set(manifest.get('historical_source_archives', [])) == archives, 'historical source archive inventory drift')
    # File ownership alone must not hide a newly added top-level component.
    for area, prefix in [('cmd', 'cmd'), ('games', 'game')]:
        for path in sorted((reference / area).iterdir()):
            if area == 'cmd' and not path.is_dir() and path.suffix not in {'.c', '.s', '.y'}:
                continue
            identifier = prefix + '.' + (path.name if path.is_dir() else path.stem)
            need(identifier in entry_map, f'missing component: {identifier}')
    for path in sorted(reference.iterdir()):
        if path.name not in {'cmd', 'games'}:
            need('lib.' + path.name in entry_map, f'missing library component: {path.name}')
    # Compare claimed implemented programs with both build and packaging declarations.
    makefile = (root / 'Makefile').read_text()
    match = re.search(r'^V7_COMMANDS\s*=\s*(.+)$', makefile, re.M)
    need(match is not None, 'cannot locate V7_COMMANDS')
    built = set(match[1].split()) | {'sh'} if match else set()
    tree = ast.parse((root / 'tools/mk_rootfs.py').read_text())
    packaged = set()
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'COMMANDS' for t in node.targets):
            packaged = set(ast.literal_eval(node.value))
    if any(isinstance(node, ast.Constant) and node.value == '/etc/init' for node in ast.walk(tree)):
        packaged.add('init')
    need(installed == built == packaged, f'implemented output drift: manifest={sorted(installed)}, build={sorted(built)}, packaged={sorted(packaged)}')
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', nargs='?', type=Path, default=ROOT / 'docs/v7-userland-manifest.json')
    args = parser.parse_args()
    try:
        manifest = json.loads(args.manifest.read_text())
        errors = validate(manifest)
    except (OSError, ValueError, TypeError, KeyError) as error:
        sys.exit(f'FAIL: {error}')
    if errors:
        sys.exit('FAIL: inventory validation\n' + '\n'.join(errors))
    count = len({source for entry in manifest['entries'] for source in entry['sources']})
    installed = {output['name'] for entry in manifest['entries'] for output in entry['outputs']
                 if output['status'] == 'implemented'}
    print(f'PASS: V7 inventory: {len(manifest["entries"])} components, {count} reference files, '
          f'{len(manifest["requirements"])} external/platform requirements, {len(installed)} installed programs')


if __name__ == '__main__':
    main()
