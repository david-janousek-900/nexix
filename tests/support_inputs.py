"""Validate missing-input dispositions and execute the extended-byte lex driver."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
CC='cc'

class SupportInputs(unittest.TestCase):
    def test_all_inputs_have_dispositions(self):
        document=json.loads((ROOT/'docs/support-inputs.json').read_text())
        self.assertEqual(document['schema_version'],1)
        self.assertEqual({e['id'] for e in document['inputs']},{'bc-math','lex-ratfor','lex-extended'})
        for record in document['inputs']:
            self.assertTrue(record['note'] and record['consumer_tasks'])
            for key in ['native','origin']:
                path=Path(record[key]);self.assertFalse(path.is_absolute());self.assertNotIn('..',path.parts)
                self.assertTrue((ROOT/path).is_file())

    def test_bc_library_has_all_six_functions(self):
        source=(ROOT/'user/v7/bc/lib.b').read_text()
        self.assertEqual(source,(ROOT/'reference/supplemental/v6/lib.b').read_text())
        self.assertEqual(set(re.findall(r'^define ([a-z])\(',source,re.M)),set('esclaj'))
        depth=0
        for char in source:
            if char=='{':depth+=1
            if char=='}':depth-=1
            self.assertGreaterEqual(depth,0)
        self.assertEqual(depth,0)

    def test_ratfor_common_contract(self):
        source=(ROOT/'user/v7/lex/nrform').read_text()
        expected='common /yyldat/ yyfnd, yymorf, yyprev, yybgin, yylsp, yylsta'
        self.assertIn(expected,(ROOT/'reference/v7/usr/src/cmd/lex/header.c').read_text())
        records=[line for line in source.splitlines() if line.startswith('common /yyldat/')]
        self.assertEqual(records,[expected,expected])
        self.assertNotRegex(source,r'\byytop\b|\bymorf\b')
        self.assertIn('integer yyfnd, yymorf, yylsta(YYLMAX), yylsp, yytext, yyprev, yyleng',source)
        self.assertIn('YYTOPVAL',source)
        self.assertIn('CDDL HEADER START',source)

    def test_extended_scanner(self):
        contents=''.join((ROOT/name).read_text() for name in [
            'tests/lex-support-prefix.c','user/v7/lex/ebcform','tests/lex-support-suffix.c'])
        with tempfile.TemporaryDirectory() as temp:
            source=Path(temp)/'scanner.c';source.write_text(contents)
            for char_mode in ['-fsigned-char','-funsigned-char']:
                for optimized in [False,True]:
                    binary=Path(temp)/'scanner'
                    command=[CC,'-std=gnu11','-Wno-implicit-int','-Wno-implicit-function-declaration',
                        '-Wno-return-type',char_mode,'-fsanitize=undefined','-fno-sanitize-recover=all']
                    if optimized:command+=['-DYYOPTIM=1']
                    subprocess.run(command+[str(source),'-o',str(binary)],check=True,capture_output=True)
                    result=subprocess.run([str(binary)],check=True,capture_output=True,text=True,timeout=5)
                    self.assertIn('all 255 non-NUL bytes',result.stdout)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--cc',default='cc');args,remaining=parser.parse_known_args()
    CC=args.cc;unittest.main(argv=[__file__]+remaining)
