import copy
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from mkconf import configure,DRIVERS
ROOT=Path(__file__).resolve().parents[1]

class Mkconf(unittest.TestCase):
    def setUp(self):self.config=json.loads((ROOT/'config/kernel.json').read_text())
    def test_native_configuration(self):
        text=configure(self.config)
        for obj,source in DRIVERS.values():
            self.assertTrue((ROOT/source).is_file())
            self.assertEqual(text.count(obj),1)
        self.config['drivers'].reverse()
        self.assertEqual(text,configure(self.config))
    def test_invalid(self):
        for key,value in [('architecture','pdp11'),('memory','swap'),('root','hp'),
                          ('schema_version',2),('unknown','x'),('drivers',['console']),
                          ('drivers',['console']*5),('drivers',['console',{}])]:
            config=copy.deepcopy(self.config);config[key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):configure(config)

if __name__=='__main__':unittest.main()
