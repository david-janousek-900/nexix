"""Installation transactions, provenance, guest metadata and root equivalence."""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import install_manifest as install
from mk_rootfs import build


class Installation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.stage = self.base / 'stage'
        self.manifest = self.base / 'install.json'
        self.plan = json.loads(install.MANIFEST.read_text())
        self.save()

    def save(self):
        self.manifest.write_text(json.dumps(self.plan))

    def stage_it(self):
        return install.stage_tree(self.stage, self.manifest)

    def test_reproducible_stage_and_root(self):
        self.assertEqual(self.stage_it(), sum(e["type"] == "file" for e in self.plan["entries"]))
        receipt = (self.stage / 'receipt.json').read_bytes()
        records, files = install.read_stage(self.stage, self.manifest)
        self.assertEqual(len(files), sum(e["type"] == "file" for e in self.plan["entries"]))
        self.assertEqual(files['/bin/sh'], Path('build/v7-sh.elf').read_bytes())
        self.assertEqual(next(e for e in records if e['path'] == '/bin/sh')['component'], 'cmd.sh')
        self.assertEqual(self.stage_it(), sum(e["type"] == "file" for e in self.plan["entries"]))
        self.assertEqual(receipt, (self.stage / 'receipt.json').read_bytes())
        direct = build(None, Path('build'), Path('rootfs'))
        staged = build(None, Path('/unused'), Path('/unused'), self.manifest, self.stage)
        self.assertEqual(staged, direct)
        bootstrap=build(1024,Path('build'),install.BOOTSTRAP_CONFIG,install.BOOTSTRAP,inode_blocks=12)
        self.assertEqual(bootstrap, Path('build/root.img').read_bytes())
        self.assertFalse((self.stage / 'root/dev/console').exists())

    def test_installed_scripts_preserve_reference_bytes(self):
        self.stage_it()
        entries,payloads=install.read_stage(self.stage,self.manifest)
        owners={e['id']:e for e in json.loads(install.INVENTORY.read_text())['entries']}
        for e in entries:
            owner=owners[e['component']]
            if owner['kind']=='installed_script':
                self.assertEqual(payloads[e['path']],(install.ROOT/owner['source_files'][0]).read_bytes())
                self.assertTrue(int(e['mode'],8)&0o111)

    def test_nonexecutable_script_is_rejected(self):
        next(e for e in self.plan['entries'] if e['path']=='/bin/false')['mode']='0644'
        self.save()
        with self.assertRaisesRegex(ValueError,'invalid executable script'):
            install.load(self.manifest)

    def test_metadata_and_hardlinks_reach_disk(self):
        config = self.base / 'config'; config.mkdir(); (config / 'payload').write_bytes(b'guest data')
        common = dict(mode='0755', uid=123, gid=456, component='distribution.config')
        self.plan['entries'] = [dict(path='/', type='directory', **common),
            dict(path='/home', type='directory', **dict(common, mode='0700')),
            dict(path='/home/data', type='file', format='data', source=dict(root='config', path='payload'), **dict(common, mode='4750')),
            dict(path='/device', type='character', device=0x1234, **common),
            dict(path='/alias', type='link', target='/home/data', component='distribution.config')]
        self.save();install.stage_tree(self.stage, self.manifest, config=config)
        image = build(256, None, None, self.manifest, self.stage)
        def inode(n): return struct.unpack_from('<HHHH', image, 1024+(n-1)*64)
        self.assertEqual(inode(2), (0o40755, 3, 123, 456))
        self.assertEqual(inode(3), (0o40700, 2, 123, 456))
        self.assertEqual(inode(4), (0o104750, 2, 123, 456))
        self.assertEqual(inode(5), (0o20755, 1, 123, 456))
        self.assertEqual((self.stage/'root/home/data').stat().st_mode & 0o7777, 0o750)

    def test_explicit_fixture_overrides_preserve_verified_stage(self):
        self.stage_it()
        receipt=(self.stage/'receipt.json').read_bytes()
        original=build(None,None,None,self.manifest,self.stage)
        fixture=build(None,None,None,self.manifest,self.stage,fixture_overrides={'/etc/ttys':b''})
        self.assertNotEqual(fixture,original)
        self.assertEqual(build(None,None,None,self.manifest,self.stage),original)
        self.assertEqual(install.read_stage(self.stage,self.manifest)[1]['/etc/ttys'],Path('rootfs/etc/ttys').read_bytes())
        self.assertEqual((self.stage/'receipt.json').read_bytes(),receipt)
        for overrides in ({'/bin/sh':b'fake'},{'/missing':b''},{'/etc/ttys':'text'}):
            with self.assertRaises(ValueError):build(None,None,None,self.manifest,self.stage,fixture_overrides=overrides)

    def test_removes_only_verified_previous_outputs(self):
        self.stage_it()
        self.plan['entries'] = [e for e in self.plan['entries'] if e['path'] != '/.profile']
        self.save();self.stage_it()
        self.assertFalse((self.stage/'root/.profile').exists())
        install.read_stage(self.stage, self.manifest)

    def test_modified_payload_refuses_replacement(self):
        self.stage_it();target = self.stage/'root/etc/passwd';target.write_bytes(b'local change')
        with self.assertRaisesRegex(ValueError, 'modified stage'):
            self.stage_it()
        self.assertEqual(target.read_bytes(), b'local change')

    def test_unknown_file_refuses_replacement(self):
        self.stage_it();target = self.stage/'notes';target.write_text('keep')
        with self.assertRaisesRegex(ValueError, 'unexpected stage'):
            self.stage_it()
        self.assertEqual(target.read_text(), 'keep')

    def test_unmanaged_destination_is_preserved(self):
        self.stage.mkdir();(self.stage/'notes').write_text('keep')
        with self.assertRaises(OSError): self.stage_it()
        self.assertEqual((self.stage/'notes').read_text(), 'keep')

    def test_missing_input_preserves_existing_stage(self):
        self.stage_it();before = (self.stage/'receipt.json').read_bytes()
        with self.assertRaises(OSError):
            install.stage_tree(self.stage, self.manifest, programs=self.base/'missing')
        self.assertEqual((self.stage/'receipt.json').read_bytes(), before)
        install.read_stage(self.stage, self.manifest)

    def test_source_and_destination_symlinks_rejected(self):
        other = self.base/'other';other.mkdir();link = self.base/'link';link.symlink_to(other, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'symlink'):
            install.stage_tree(link, self.manifest)
        with self.assertRaisesRegex(ValueError, 'symlink'):
            install.stage_tree(self.stage, self.manifest, programs=link)
        self.stage_it();target = self.stage/'root/.profile';target.unlink();target.symlink_to(Path('rootfs/profile').resolve())
        with self.assertRaisesRegex(ValueError, 'symlink'): install.read_stage(self.stage, self.manifest)

    def test_stale_or_forged_receipt_rejected(self):
        self.stage_it();self.plan['entries'][0]['mode'] = '0700';self.save()
        with self.assertRaisesRegex(ValueError, 'differ|stale'):
            install.read_stage(self.stage, self.manifest)
        self.stage_it();path = self.stage/'receipt.json';receipt=json.loads(path.read_text())
        receipt['entries'][0]['uid']=99;path.write_text(json.dumps(receipt))
        with self.assertRaisesRegex(ValueError, 'differ'):
            install.read_stage(self.stage, self.manifest)

    def test_manifest_errors(self):
        baseline = copy.deepcopy(self.plan)
        mutations = [lambda e: e.append(copy.deepcopy(e[-1])),
            lambda e: e[0].update(path='/../bad'),
            lambda e: e[0].update(mode='9999'),
            lambda e: e[0].update(uid=-1),
            lambda e: e[0].update(component='unknown'),
            lambda e: e[7]['source'].update(path='../escape'),
            lambda e: e[7].update(path='/missing/init'),
            lambda e: e[7].update(component='cmd.login'),
            lambda e: e.append(dict(path='/cycle',type='link',target='/cycle',component='distribution.config'))]
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                self.plan=copy.deepcopy(baseline);mutate(self.plan['entries']);self.save()
                with self.assertRaises((ValueError,KeyError)): install.load(self.manifest)


if __name__ == '__main__':
    unittest.main()
