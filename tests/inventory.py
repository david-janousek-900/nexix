"""Check inventory failures that would otherwise hide missing port work."""
import copy
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from check_inventory import validate


class Inventory(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = json.loads((ROOT / 'docs/v7-userland-manifest.json').read_text())

    def setUp(self):
        self.manifest = copy.deepcopy(self.original)

    def entry(self, identifier):
        return next(e for e in self.manifest['entries'] if e['id'] == identifier)

    def reject(self, diagnostic):
        self.assertTrue(any(diagnostic in error for error in validate(self.manifest)), diagnostic)

    def test_current_inventory(self):
        self.assertEqual(validate(self.manifest), [])

    def test_unclassified_source(self):
        source = 'reference/v7/usr/src/cmd/cat.c'
        self.entry('cmd.cat')['sources'].remove(source)
        self.reject('unclassified reference file: ' + source)

    def test_shared_ownership_cannot_hide_missing_component(self):
        self.manifest['entries'].remove(self.entry('cmd.cat'))
        self.entry('cmd.echo')['sources'].append('reference/v7/usr/src/cmd/cat.c')
        self.reject('missing component: cmd.cat')

    def test_duplicate_identifiers(self):
        self.manifest['entries'].append(copy.deepcopy(self.entry('cmd.cat')))
        self.reject('duplicate identifier: cmd.cat')

    def test_unknown_dependency(self):
        self.entry('cmd.cat')['dependencies'][0]['target'] = 'invented-tool'
        self.reject('unknown dependency invented-tool')

    def test_confirmed_dependency_requires_evidence(self):
        dependency = self.entry('cmd.cat')['dependencies'][0]
        dependency['certainty'] = 'confirmed'
        dependency['evidence'] = []
        self.reject('confirmed dependency needs evidence')

    def test_deleted_evidence(self):
        self.entry('cmd.cat')['outputs'][0]['evidence'] = ['reference/v7/does-not-exist']
        self.reject('missing path reference/v7/does-not-exist')

    def test_source_cannot_escape_repository(self):
        self.entry('cmd.cat')['sources'].append('../outside.c')
        self.reject('unsafe path ../outside.c')

    def test_implemented_status_must_match_packaging(self):
        self.entry('cmd.date')['outputs'][0]['status'] = 'implemented'
        self.reject('implemented output drift')

    def test_missing_external_requirement_is_visible(self):
        self.manifest['requirements'] = [r for r in self.manifest['requirements'] if r['id'] != 'yacc-skeleton']
        self.reject('unknown dependency yacc-skeleton')

    def test_acceptance_is_required(self):
        self.entry('cmd.cat')['acceptance'] = []
        self.reject('no acceptance checks')


if __name__ == '__main__':
    unittest.main()
