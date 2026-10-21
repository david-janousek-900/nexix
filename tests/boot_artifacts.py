"""Protect boot/kernel provenance from omissions and false native claims."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from check_boot_artifacts import validate


class BootArtifacts(unittest.TestCase):
    def setUp(self):
        self.audit = json.loads((ROOT / 'docs/boot-kernel-artifacts.json').read_text())

    def test_current(self):
        self.assertEqual(validate(ROOT, self.audit), [])

    def test_omission_and_duplicate(self):
        self.audit['artifacts'][-1] = self.audit['artifacts'][0]
        self.assertIn('incomplete or duplicate artifact coverage', validate(ROOT, self.audit))

    def test_drift_and_false_claims(self):
        cases = [('sha256', '0' * 64, 'hash drift'), ('size', 0, 'size drift'),
                 ('disposition', 'native_executable', 'false execution claim'),
                 ('native_artifact', 'reference/v7/unix', 'wrong native association'),
                 ('native_sources', ['../etc/passwd'], 'unsafe evidence'),
                 ('native_sources', ['reference/v7/unix'], 'classification drift'),
                 ('historical_evidence', [], 'missing historical_evidence')]
        for field, value, diagnostic in cases:
            with self.subTest(field=field, value=value):
                self.setUp()
                self.audit['artifacts'][0][field] = value
                self.assertTrue(any(diagnostic in e for e in validate(ROOT, self.audit)))

    def test_malformed_records(self):
        for records in (None, [None], [{'source': []}]):
            with self.subTest(records=records):
                self.audit['artifacts'] = records
                self.assertTrue(validate(ROOT, self.audit))


if __name__ == '__main__':
    unittest.main()
