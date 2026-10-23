"""Reject omissions and misleading completion claims in roadmap coverage."""
import copy
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from check_roadmap import validate_roadmap


class Roadmap(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original_manifest = json.loads((ROOT / 'docs/v7-userland-manifest.json').read_text())
        cls.original_coverage = json.loads((ROOT / 'docs/roadmap-coverage.json').read_text())
        cls.original_roadmap_text = (ROOT / 'docs/tasks.md').read_text()

    def setUp(self):
        self.manifest = copy.deepcopy(self.original_manifest)
        self.coverage = copy.deepcopy(self.original_coverage)
        self.roadmap_text = self.original_roadmap_text

    def reject(self, diagnostic):
        errors = validate_roadmap(self.manifest, self.coverage, self.roadmap_text)
        self.assertTrue(any(diagnostic in error for error in errors), errors)

    def test_current_coverage(self):
        self.assertEqual(validate_roadmap(self.manifest, self.coverage, self.roadmap_text), [])

    def test_extension_tasks_need_ownership(self):
        del self.coverage['extensions']
        self.reject('unowned roadmap task: 412')

    def test_invalid_extension_mapping(self):
        self.coverage['extensions'] = []
        self.reject('roadmap extensions mapping must be an object')

    def test_invalid_extension_references(self):
        for value in [[], [True], [432], '412']:
            with self.subTest(value=value):
                self.coverage['extensions'] = {'tcp-ip-0.2.0': value}
                self.reject('extension tasks must be a nonempty list of known task IDs')

    def test_extension_references_must_be_sorted_and_unique(self):
        self.coverage['extensions']['tcp-ip-0.2.0'] = [413, 412, 412]
        self.reject('extension references must be sorted and unique')

    def test_unresolved_work_needs_explicit_owners(self):
        entry = next(e for e in self.manifest['entries'] if e['id'] == 'cmd.cat')
        del entry['unresolved_tasks']
        self.reject('cmd.cat: unresolved work needs explicit pending task owners')

    def test_unresolved_work_cannot_use_done_owner(self):
        entry = next(e for e in self.manifest['entries'] if e['id'] == 'cmd.cat')
        entry['unresolved_tasks'] = [16]
        self.reject('cmd.cat: unresolved work cannot have completed owner 16')

    def test_unresolved_owner_must_belong_to_component(self):
        entry = next(e for e in self.manifest['entries'] if e['id'] == 'cmd.cat')
        for value in [[174], [True], '389', [None]]:
            with self.subTest(value=value):
                entry['unresolved_tasks'] = value
                self.reject('cmd.cat: unresolved_tasks must reference mapped owner tasks')

    def test_unresolved_owners_must_be_sorted_and_unique(self):
        entry = next(e for e in self.manifest['entries'] if e['id'] == 'cmd.cat')
        entry['unresolved_tasks'] = [389, 389]
        self.reject('cmd.cat: unresolved task owners must be sorted and unique')

    def test_clearing_obligation_also_clears_owners(self):
        entry = next(e for e in self.manifest['entries'] if e['id'] == 'cmd.cat')
        entry['unresolved'] = []
        self.reject('cmd.cat: unresolved task owners without unresolved work')

    def test_scope_limits_are_not_unfinished_work(self):
        entry = next(e for e in self.manifest['entries'] if e['id'] == 'cmd.date')
        self.assertTrue(entry['scope_notes'])
        self.assertFalse(entry['unresolved'])
        self.assertEqual(validate_roadmap(self.manifest, self.coverage, self.roadmap_text), [])
        entry['scope_notes'] = [None]
        self.reject('cmd.date: scope_notes must be nonempty text entries')

    def test_requirements_also_need_obligation_owners(self):
        entry = next(e for e in self.manifest['requirements'] if e['id'] == 'manual-pages')
        entry['unresolved'] = ['A newly discovered consumer gap.']
        self.reject('manual-pages: unresolved work needs explicit pending task owners')

    def test_missing_component_mapping(self):
        del self.coverage['components']['script.man']
        self.reject('unmapped roadmap components: script.man')

    def test_missing_requirement_mapping(self):
        del self.coverage['requirements']['manual-pages']
        self.reject('unmapped roadmap requirements: manual-pages')

    def test_unknown_component(self):
        self.coverage['components']['cmd.invented'] = [119]
        self.reject('unknown roadmap components: cmd.invented')

    def test_unknown_requirement(self):
        self.coverage['requirements']['invented-input'] = [88]
        self.reject('unknown roadmap requirements: invented-input')

    def test_empty_component_mapping(self):
        self.coverage['components']['cmd.ed'] = []
        self.reject('cmd.ed: roadmap tasks must be a nonempty list')

    def test_invalid_reference_types_and_bounds(self):
        for value in [True, '145', None, 0, -1, self.coverage['task_count'] + 1]:
            with self.subTest(value=value):
                self.coverage['components']['cmd.ed'] = [value]
                self.reject('cmd.ed: invalid roadmap task reference')

    def test_duplicate_reference(self):
        self.coverage['components']['cmd.ed'] = [145, 145]
        self.reject('references must be sorted and unique')

    def test_unordered_references(self):
        self.coverage['components']['cmd.awk'] = [176, 174, 175]
        self.reject('references must be sorted and unique')

    def test_orphan_task(self):
        for field in ['components', 'requirements']:
            for tasks in self.coverage[field].values():
                if 145 in tasks:
                    tasks.remove(145)
        self.reject('unowned roadmap task: 145')

    def test_missing_roadmap_row(self):
        self.roadmap_text = '\n'.join(line for line in self.roadmap_text.splitlines() if not line.startswith('| 145 |'))
        self.reject('IDs must be unique, ordered and contiguous')

    def test_duplicate_roadmap_row(self):
        row = next(line for line in self.roadmap_text.splitlines() if line.startswith('| 145 |'))
        self.roadmap_text += '\n' + row
        self.reject('IDs must be unique, ordered and contiguous')

    def test_malformed_roadmap_status(self):
        self.roadmap_text = self.roadmap_text.replace('| 145 | Port ed | Done |', '| 145 | Port ed | Complete |')
        self.reject('malformed roadmap row:')

    def test_status_summary_drift(self):
        self.roadmap_text = self.roadmap_text.replace('| 145 | Port ed | Done |', '| 145 | Port ed | In progress |')
        self.reject('roadmap status summary drift')

    def test_unfinished_component_cannot_hide_behind_done_task(self):
        self.coverage['components']['cmd.ratfor'] = [16]
        self.reject('cmd.ratfor: unfinished component cannot map only to completed tasks')

    def test_implemented_component_needs_completed_task(self):
        self.coverage['components']['cmd.cat'] = [203]
        self.reject('cmd.cat: implemented component needs a completed roadmap task')

    def test_done_task_needs_commit_evidence(self):
        self.roadmap_text = self.roadmap_text.replace('`09ce60d`', 'no commit recorded')
        self.reject('roadmap task 5: completed task needs commit evidence')

    def test_requirement_supply_is_not_native_completion(self):
        requirement = next(r for r in self.manifest['requirements'] if r['id'] == 'manual-pages')
        requirement['status'] = 'supplied'
        self.assertEqual(validate_roadmap(self.manifest, self.coverage, self.roadmap_text), [])

    def test_bad_coverage_shape(self):
        self.coverage['components'] = []
        self.reject('roadmap components mapping must be an object')

    def test_schema_and_version_drift(self):
        self.coverage['schema_version'] = 99
        self.coverage['version'] = '0.3.0'
        self.reject('unsupported roadmap coverage schema')
        self.reject('roadmap version drift')

    def test_missing_task_count(self):
        del self.coverage['task_count']
        self.reject('invalid roadmap task count')

    def test_legacy_schema_requires_migration(self):
        self.coverage['schema_version'] = 1
        self.reject('unsupported roadmap coverage schema')

    def test_legacy_count_field_requires_migration(self):
        self.coverage['step_count'] = self.coverage.pop('task_count')
        self.reject('invalid roadmap task count')

    def test_declared_task_count_cannot_hide_missing_rows(self):
        self.coverage['task_count'] += 1
        self.reject('IDs must be unique, ordered and contiguous')

    def test_declared_paths_are_pinned(self):
        self.coverage['roadmap'] = '../outside.md'
        self.coverage['inventory'] = 'another-inventory.json'
        self.reject('unexpected roadmap path')
        self.reject('unexpected roadmap inventory path')

    def test_missing_summary(self):
        self.roadmap_text = self.roadmap_text.replace('tracking tasks:', 'untracked tasks:')
        self.reject('missing roadmap status summary')


if __name__ == '__main__':
    unittest.main()
