"""Exercise historical word ordering, alignment, safety and all pinned inputs."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from v7_archive import ROOT, members, extract


def archive(name=b'test.c', payload=b'abc'):
    # PDP-11 longs have the high 16-bit word first; each word is little endian.
    return (b'\x65\xff' + name.ljust(14, b'\0') + struct.pack('<HHBBHHH',
            0x1234, 0x5678, 7, 8, 0o644, len(payload) >> 16, len(payload) & 65535)
            + payload + (b'\0' if len(payload) & 1 else b''))


class ArchiveTest(unittest.TestCase):
    def test_layout(self):
        record, payload = members(archive())[0]
        self.assertEqual(payload, b'abc')
        self.assertEqual(record['mtime'], 0x12345678)
        self.assertEqual((record['uid'], record['gid'], record['mode']), (7, 8, 0o644))
        self.assertEqual(members(archive(payload=b'x' * 65537))[0][0]['size'], 65537)

    def test_reject(self):
        for data in (b'', b'!<arch>\n', archive()[:-1], archive()[:20],
                     archive(b'../escape'), archive(b'/absolute'), archive(b'..'),
                     archive(b'x\\y'), archive(b'bad\nname'), archive() + archive()[2:]):
            with self.subTest(data=data[:30]), self.assertRaises(ValueError):
                members(data)

    def test_all_reference_archives(self):
        inventory = json.loads((ROOT / 'docs/v7-userland-manifest.json').read_text())
        sources = inventory['historical_source_archives'] + inventory['data_archives']
        self.assertEqual(len(sources), 12)
        with tempfile.TemporaryDirectory() as temp:
            for name in sources:
                source = ROOT / name
                parsed = members(source.read_bytes())
                self.assertTrue(parsed)
                destination = Path(temp) / source.name
                records = extract(source, destination)
                self.assertEqual(len(list(destination.iterdir())), len(records))
                for record, payload in parsed:
                    actual = (destination / record['name']).read_bytes()
                    self.assertEqual(actual, payload)
                    self.assertEqual(hashlib.sha256(actual).hexdigest(), record['sha256'])
                with self.assertRaises(ValueError):
                    extract(source, destination)


if __name__ == '__main__':
    unittest.main()
