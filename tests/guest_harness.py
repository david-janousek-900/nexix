import json
import re
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from guest_acceptance import load_suite,run_suite

class Harness(unittest.TestCase):
    def test_bad_suites(self):
        for step in ({'command':'a\nb'},{'command':'x','status':256},{'expect':'['},{'expect':'x','terminal':2},{}):
            with tempfile.TemporaryDirectory() as temp:
                path=Path(temp)/'suite';path.write_text(json.dumps({'schema_version':1,'cases':[{'id':'case','component':'cmd.echo','steps':[step]}]}))
                with self.assertRaises((ValueError,KeyError,re.error)):load_suite(path)
    def test_missing_component(self):
        with self.assertRaisesRegex(ValueError,'no selected'):run_suite('tests/guest/foundation.json','build/unused-report.json',components=['missing'])

if __name__=='__main__':unittest.main()
