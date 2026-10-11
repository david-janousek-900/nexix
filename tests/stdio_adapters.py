"""Check complete private-stdio consumer coverage and source drift rejection."""
from pathlib import Path
import sys
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from adapt_v7_stdio import ADAPTERS, PRIVATE, adapt_source, staged_sources, test_fragments


class Adapters(unittest.TestCase):
    def test_complete_consumers(self):
        base = ROOT / 'reference/v7/usr/src'
        actual = {str(p.relative_to(base)) for p in base.rglob('*')
                  if p.suffix in {'.c', '.h'} and not p.relative_to(base).parts[0]=='libc'
                  and PRIVATE.search(p.read_text())}
        self.assertEqual(actual, set(ADAPTERS))

    def test_no_private_interfaces_remain(self):
        for name, text in staged_sources().items():
            with self.subTest(source=name): self.assertIsNone(PRIVATE.search(text))

    def test_omitted_and_duplicated_reference_fragment(self):
        base = ROOT / 'reference/v7/usr/src'
        for name, pairs in ADAPTERS.items():
            original = (base / name).read_text()
            old = pairs[0][0]
            for changed in (original.replace(old, ''), original + '\n'+old):
                with self.subTest(source=name):
                    with self.assertRaises(ValueError): adapt_source(name, changed)

    def test_new_private_dependency_rejected(self):
        name = 'games/quiz.c'
        text = (ROOT / 'reference/v7/usr/src' / name).read_text()
        with self.assertRaises(ValueError): adapt_source(name, text+'\nstdin->_cnt = 0;')

    def test_runtime_fragments_follow_adapters(self):
        sources = staged_sources()
        fragments = test_fragments(sources)
        self.assertIsNone(PRIVATE.search(fragments))
        for routine in ('v7_stdio_gdate', 'v7_stdio_refer_output', 'v7_stdio_refer_input',
                        'v7_stdio_quiz_output', 'v7_stdio_cpp_output'):
            self.assertIn(routine, fragments)
        del sources['games/quiz.c']
        with self.assertRaises(KeyError): test_fragments(sources)


if __name__ == '__main__': unittest.main()
