"""Verify separately recovered source bytes and immutable provenance URLs."""
import hashlib
import json
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]
class Supplemental(unittest.TestCase):
    def test_provenance(self):
        document=json.loads((ROOT/'reference/supplemental/provenance.json').read_text())
        self.assertEqual(document['schema_version'],1)
        paths=set()
        for record in document['inputs']:
            path=record['path']
            self.assertTrue(path.startswith('reference/supplemental/') and '..' not in Path(path).parts)
            self.assertNotIn(path,paths);paths.add(path)
            self.assertEqual(hashlib.sha256((ROOT/path).read_bytes()).hexdigest(),record['sha256'])
            self.assertRegex(record['url'],r'https://raw.githubusercontent.com/[^/]+/[^/]+/[0-9a-f]{40}/')
            self.assertTrue((ROOT/record['license']).is_file())
        actual={str(p.relative_to(ROOT)) for p in (ROOT/'reference/supplemental').rglob('*') if p.is_file() and p.name!='provenance.json'}
        self.assertEqual(paths,actual)
if __name__=='__main__':unittest.main()
