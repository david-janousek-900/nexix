"""Verify every separately imported traditional vi source byte."""
import hashlib,json
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class Reference(unittest.TestCase):
 def test_index(self):
  ledger=json.loads((ROOT/'reference/ex-vi/provenance.json').read_text());base=ROOT/'reference/ex-vi/050325'
  self.assertEqual(ledger['version'],'050325');seen=set()
  for r in ledger['files']:
   p=Path(r['path']);self.assertFalse(p.is_absolute());self.assertNotIn('..',p.parts);self.assertNotIn(str(p),seen);seen.add(str(p))
   data=(base/p).read_bytes();self.assertEqual(len(data),r['size']);self.assertEqual(hashlib.sha256(data).hexdigest(),r['sha256'])
  self.assertEqual(seen,{str(p.relative_to(base)) for p in base.rglob('*') if p.is_file()})
if __name__=='__main__':unittest.main()
