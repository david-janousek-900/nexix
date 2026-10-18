"""Require an explicit native responsibility mapping for every libc reference input."""
import copy
import json
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]

def errors(document,expected):
    found=[];result=[]
    if document.get('schema_version')!=1 or not document.get('policy'):result.append('schema')
    for record in document.get('sources',[]):
        found.append(record.get('source'))
        if not record.get('note') or not record.get('native'):result.append('mapping')
        for name in record.get('native',[]):
            path=Path(name)
            if path.is_absolute() or '..' in path.parts or not (ROOT/path).is_file():result.append('native path')
    if len(found)!=len(set(found)) or set(found)!=expected:result.append('coverage')
    return result

class Coverage(unittest.TestCase):
    def test_complete_and_corruptions(self):
        inventory=json.loads((ROOT/'docs/v7-userland-manifest.json').read_text())
        expected=set(next(e for e in inventory['entries'] if e['id']=='lib.libc')['sources'])
        document=json.loads((ROOT/'docs/libc-source-coverage.json').read_text())
        self.assertEqual(errors(document,expected),[])
        for mutation in ['omit','duplicate','path','note']:
            bad=copy.deepcopy(document)
            if mutation=='omit':bad['sources'].pop()
            if mutation=='duplicate':bad['sources'].append(bad['sources'][0])
            if mutation=='path':bad['sources'][0]['native']=['../outside']
            if mutation=='note':bad['sources'][0]['note']=''
            self.assertTrue(errors(bad,expected))
        print(f'PASS: {len(expected)} libc reference inputs have explicit native replacements')
if __name__=='__main__':unittest.main()
