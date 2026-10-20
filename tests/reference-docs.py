"""Reject stale review counts and supporting-requirement completion claims."""
import copy
import json
from pathlib import Path
import sys
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from check_reference_docs import validate


class ReferenceDocs(unittest.TestCase):
    def setUp(self):
        read = lambda name: (ROOT / 'docs' / name).read_text()
        self.args = [json.loads(read('v7-userland-manifest.json')),
                     json.loads(read('roadmap-coverage.json')), read('tasks.md'),
                     read('reference-review.md'), read('porting-inventory.md')]

    def test_current(self):
        self.assertEqual(validate(*self.args), [])

    def test_totals_change_with_manifest_or_tasks(self):
        for mutation in ('component', 'requirement', 'task', 'state'):
            with self.subTest(mutation=mutation):
                args = copy.deepcopy(self.args)
                if mutation == 'component':
                    args[0]['entries'].pop()
                elif mutation == 'requirement':
                    args[0]['requirements'].pop()
                elif mutation == 'task':
                    args[1]['task_count'] += 1
                else:
                    args[2] = args[2].replace('| 389 | Close cat V7 compatibility audit | To do |',
                                              '| 389 | Close cat V7 compatibility audit | Done |')
                self.assertIn('reference review totals differ from canonical inventory/roadmap', validate(*args))

    def test_stale_requirement_state_or_description(self):
        for key, value in [('status', 'missing'), ('description', 'Newly completed implementation.')]:
            with self.subTest(key=key):
                args = copy.deepcopy(self.args)
                args[0]['requirements'][0][key] = value
                self.assertIn('documented requirement table differs from canonical inventory', validate(*args))

    def test_missing_or_duplicate_marker(self):
        self.args[4] = self.args[4].replace('<!-- requirements:start -->', '')
        self.assertIn('requirement table needs exactly one marker pair', validate(*self.args))

    def test_duplicate_totals(self):
        summary = next(line for line in self.args[3].splitlines() if line.startswith('Reference audit totals:'))
        self.args[3] += '\n' + summary
        self.assertTrue(validate(*self.args))


if __name__ == '__main__':
    unittest.main()
