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
        cls.original_readme = (ROOT / 'README.md').read_text()

    def setUp(self):
        self.manifest = copy.deepcopy(self.original_manifest)
        self.coverage = copy.deepcopy(self.original_coverage)
        self.readme = self.original_readme

    def reject(self, diagnostic):
        errors = validate_roadmap(self.manifest, self.coverage, self.readme)
        self.assertTrue(any(diagnostic in error for error in errors), errors)

    def test_current_coverage(self):
        self.assertEqual(validate_roadmap(self.manifest, self.coverage, self.readme), [])

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
        self.reject('cmd.ed: roadmap steps must be a nonempty list')

    def test_invalid_reference_types_and_bounds(self):
        for value in [True, '145', None, 0, -1, 362]:
            with self.subTest(value=value):
                self.coverage['components']['cmd.ed'] = [value]
                self.reject('cmd.ed: invalid roadmap step reference')

    def test_duplicate_reference(self):
        self.coverage['components']['cmd.ed'] = [145, 145]
        self.reject('references must be sorted and unique')

    def test_unordered_references(self):
        self.coverage['components']['cmd.awk'] = [176, 174, 175]
        self.reject('references must be sorted and unique')

    def test_orphan_step(self):
        for field in ['components', 'requirements']:
            for steps in self.coverage[field].values():
                if 145 in steps:
                    steps.remove(145)
        self.reject('unowned roadmap step: 145')

    def test_missing_readme_row(self):
        self.readme = '\n'.join(line for line in self.readme.splitlines() if not line.startswith('| 145 |'))
        self.reject('IDs must be unique, ordered and contiguous')

    def test_duplicate_readme_row(self):
        row = next(line for line in self.readme.splitlines() if line.startswith('| 145 |'))
        self.readme += '\n' + row
        self.reject('IDs must be unique, ordered and contiguous')

    def test_malformed_readme_status(self):
        self.readme = self.readme.replace('| 145 | Port ed | To do |', '| 145 | Port ed | Complete |')
        self.reject('malformed roadmap row:')

    def test_status_summary_drift(self):
        self.readme = self.readme.replace('| 145 | Port ed | To do |', '| 145 | Port ed | In progress |')
        self.reject('roadmap status summary drift')

    def test_unfinished_component_cannot_hide_behind_done_step(self):
        self.coverage['components']['cmd.ed'] = [16]
        self.reject('cmd.ed: unfinished component cannot map only to completed steps')

    def test_implemented_component_needs_completed_step(self):
        self.coverage['components']['cmd.cat'] = [145]
        self.reject('cmd.cat: implemented component needs a completed roadmap step')

    def test_done_step_needs_commit_evidence(self):
        self.readme = self.readme.replace('`09ce60d`', 'no commit recorded')
        self.reject('roadmap step 5: completed step needs commit evidence')

    def test_requirement_supply_is_not_native_completion(self):
        requirement = next(r for r in self.manifest['requirements'] if r['id'] == 'manual-pages')
        requirement['status'] = 'supplied'
        self.assertEqual(validate_roadmap(self.manifest, self.coverage, self.readme), [])

    def test_bad_coverage_shape(self):
        self.coverage['components'] = []
        self.reject('roadmap components mapping must be an object')

    def test_schema_and_version_drift(self):
        self.coverage['schema_version'] = 99
        self.coverage['version'] = '0.2.0'
        self.reject('unsupported roadmap coverage schema')
        self.reject('roadmap version drift')

    def test_missing_step_count(self):
        del self.coverage['step_count']
        self.reject('invalid roadmap step count')

    def test_declared_step_count_cannot_hide_missing_rows(self):
        self.coverage['step_count'] = 362
        self.reject('IDs must be unique, ordered and contiguous')

    def test_declared_paths_are_pinned(self):
        self.coverage['roadmap'] = '../outside.md'
        self.coverage['inventory'] = 'another-inventory.json'
        self.reject('unexpected roadmap path')
        self.reject('unexpected roadmap inventory path')

    def test_missing_summary(self):
        self.readme = self.readme.replace('tracking steps:', 'untracked steps:')
        self.reject('missing roadmap status summary')


if __name__ == '__main__':
    unittest.main()
