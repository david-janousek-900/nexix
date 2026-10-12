"""Regression tests for omitted sources, syscall drift and false closure."""
import json
from pathlib import Path
import sys
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from check_kernel_coverage import validate


class Coverage(unittest.TestCase):
    def setUp(self):
        self.audit = json.loads((ROOT / 'docs/kernel-coverage.json').read_text())
        self.manifest = json.loads((ROOT / 'docs/v7-userland-manifest.json').read_text())
        self.readme = (ROOT / 'README.md').read_text()

    def check(self, diagnostic=None):
        errors = validate(ROOT, self.audit, self.manifest, self.readme)
        if diagnostic is None:
            self.assertEqual(errors, [])
        else:
            self.assertTrue(any(diagnostic in e for e in errors), errors)

    def test_current(self): self.check()
    def test_legacy_schema_requires_migration(self):
        self.audit['schema_version'] = 1
        self.check('unsupported')
    def test_omitted_source(self):
        self.audit['groups'][0]['sources'].pop()
        self.check('incomplete or unknown')
    def test_duplicate_source(self):
        self.audit['groups'][1]['sources'].append(self.audit['groups'][0]['sources'][0])
        self.check('duplicate kernel source')
    def test_unknown_source(self):
        self.audit['groups'][0]['sources'].append('reference/v7/usr/sys/unknown.c')
        self.check('incomplete or unknown')
    def test_inventory_drift(self):
        next(e for e in self.manifest['entries'] if e['id']=='distribution.kernel')['sources'].pop()
        self.check('inventory differs')
    def test_native_path(self):
        for path in ['kernel/nonexistent.c', '../README.md', '/etc/passwd']:
            with self.subTest(path=path):
                self.audit['groups'][0]['native'] = [path]
                self.check('missing or unsafe')
    def test_owner(self):
        self.audit['groups'][0]['tasks'] = [True]
        self.check('invalid group roadmap')
    def test_missing_slot(self):
        self.audit['syscalls'].pop()
        self.check('slots must cover')
    def test_routine_drift(self):
        self.audit['syscalls'][1]['routine'] = 'read'
        self.check('reference drift')
    def test_stub_claim(self):
        self.audit['syscalls'][21]['status'] = 'historical_stub'
        self.check('stub drift')
    def test_false_closure(self):
        self.audit['syscalls'][21]['task'] = 38
        self.check('completed owner')
    def test_schema(self):
        self.audit['schema_version'] = 99
        self.check('schema')


if __name__ == '__main__': unittest.main()
