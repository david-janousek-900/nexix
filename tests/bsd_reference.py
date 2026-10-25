"""Offline full-distribution reference integrity and fail-closed archive/import checks."""
import copy
import hashlib
import io
import json
from pathlib import Path
import shutil
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import bsd_reference as bsd


class Reference(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        base = self.root / bsd.REFERENCE
        base.mkdir(parents=True)
        for name in ('berkeley-license.html', 'audit.json'):
            shutil.copyfile(ROOT / bsd.REFERENCE / name, base / name)
        for name in bsd.SELECTED:
            target = base / 'source' / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / bsd.REFERENCE / 'source' / name, target)
        (self.root / 'LICENSES').mkdir()
        shutil.copyfile(ROOT / 'LICENSES/BSD-4.4-Lite2.txt', self.root / 'LICENSES/BSD-4.4-Lite2.txt')
        self.index = json.loads((ROOT / bsd.REFERENCE / 'provenance.json').read_text())
        self.index['files'] = [r for r in self.index['files'] if r['path'] in bsd.SELECTED]
        # Small isolated fixtures retain the complete checker behavior with their own pin.
        for name, value in [('FILE_COUNT', len(self.index['files'])),
                            ('MANIFEST_SHA256', bsd.manifest_digest(self.index))]:
            patcher = patch.object(bsd, name, value)
            patcher.start()
            self.addCleanup(patcher.stop)
        (base / 'provenance.json').write_text(json.dumps(self.index, indent=2) + '\n')
        self.source = self.root / bsd.REFERENCE / 'source'

    def reject(self, message):
        self.assertTrue(any(message in error for error in bsd.check_reference(self.index, self.root)))

    def test_reference_and_audit(self):
        self.assertEqual(bsd.check_reference(self.index, self.root), [])
        self.assertEqual(bsd.dependency_audit(self.root),
                         json.loads((self.root / bsd.REFERENCE / 'audit.json').read_text()))
        notice = (self.source / 'COPYRIGHT').read_text('latin1')
        self.assertIn('The Regents of the University of California', notice)
        self.assertIn('Redistribution and use in source and binary forms', notice)
        for name in bsd.SELECTED:
            if name == 'COPYRIGHT':
                continue
            text = (self.source / name).read_text('latin1')
            if name.startswith('usr/src/sys/'):   # kernel sources carry their own notice
                header = text.split('*/', 1)[0]
                self.assertIn('The Regents of the University of California.', header, name)
                self.assertIn('Redistribution and use in source and binary forms', header, name)
            elif 'Regents of the University of California' in text:   # layout files with a notice
                self.assertIn('Redistribution and use in source and binary forms', text, name)
            # Short layout files (Makefiles, mtree specs, MAKEDEV) fall under COPYRIGHT.

    def test_changed_bytes(self):
        (self.source / 'COPYRIGHT').write_bytes(b'changed')
        self.reject('content mismatch')

    def test_missing_and_unrecorded_files(self):
        (self.source / 'COPYRIGHT').unlink()
        (self.source / 'extra').write_bytes(b'extra')
        self.reject('missing/non-regular')
        self.reject('unrecorded files')

    def test_symlink_is_rejected(self):
        path = self.source / 'COPYRIGHT'
        path.unlink()
        path.symlink_to(self.root / 'LICENSES/BSD-4.4-Lite2.txt')
        self.reject('missing/non-regular')

    def test_invalid_index(self):
        original = copy.deepcopy(self.index)
        for value in ['../escape', '/absolute']:
            self.index = copy.deepcopy(original)
            self.index['files'][0]['path'] = value
            self.reject('unsafe or duplicate BSD reference path')
        self.index = copy.deepcopy(original)
        self.index['files'].append(self.index['files'][0])
        self.reject('duplicate')
        self.index = copy.deepcopy(original)
        self.index['commit'] = 'moving-branch'
        self.reject('invalid BSD reference provenance')

    def test_manifest_cannot_be_rewritten_to_match_changed_bytes(self):
        data = b'changed'
        (self.source / 'COPYRIGHT').write_bytes(data)
        record = self.index['files'][0]
        record['size'], record['sha256'] = len(data), bsd.digest(data)
        self.reject('manifest mismatch')

    def test_manifest_omissions_and_metadata_changes(self):
        original = copy.deepcopy(self.index)
        self.index['files'].pop()
        self.reject('manifest mismatch')
        self.index = copy.deepcopy(original)
        self.index['directories'][0]['mode'] ^= 1
        self.reject('manifest mismatch')

    def test_additive_schema_migration_preserves_existing_bytes(self):
        old = copy.deepcopy(self.index)
        old['schema_version'] = 1
        old.pop('directories')
        old.pop('link_policy')
        for record in old['files']:
            for key in ('uid', 'gid', 'mtime', 'link_target'):
                record.pop(key)
        ledger = self.root / bsd.REFERENCE / 'provenance.json'
        ledger.write_text(json.dumps(old))
        contents = {r['path']: (self.source / r['path']).read_bytes() for r in self.index['files']}
        bsd.import_reference(self.index, contents, self.root)
        self.assertEqual(json.loads(ledger.read_text()), self.index)
        self.assertEqual(bsd.check_reference(self.index, self.root), [])
        ledger.write_text(json.dumps(old))
        (self.source / 'COPYRIGHT').write_bytes(b'edited')
        with self.assertRaisesRegex(ValueError, 'changed reference bytes'):
            bsd.import_reference(self.index, contents, self.root)

    def test_license_changes(self):
        (self.root / bsd.REFERENCE / 'berkeley-license.html').write_bytes(b'changed')
        self.reject('license amendment document mismatch')
        (self.root / 'LICENSES/BSD-4.4-Lite2.txt').write_bytes(b'changed')
        self.reject('license copy mismatch')

    def test_import_preflight_has_no_partial_writes(self):
        contents = {'new': b'new', 'COPYRIGHT': b'differing bytes'}
        with self.assertRaisesRegex(ValueError, 'refusing to overwrite'):
            bsd.import_reference(self.index, contents, self.root)
        self.assertFalse((self.source / 'new').exists())

    def test_archive_hash_is_required(self):
        archive = self.root / 'bad.tar.gz'
        archive.write_bytes(b'not the pinned archive')
        with self.assertRaisesRegex(ValueError, 'archive SHA-256'):
            bsd.archive_index(archive)

    def synthetic_archive(self, names):
        archive = self.root / 'synthetic.tar.gz'
        with tarfile.open(archive, 'w:gz') as tar:
            for name, kind in names:
                member = tarfile.TarInfo(name)
                member.type = kind
                member.linkname = ('/usr/obj/example' if name.endswith('absolute-link') else 'elsewhere') if kind == tarfile.SYMTYPE else ''
                member.size = 1 if kind == tarfile.REGTYPE else 0
                tar.addfile(member, io.BytesIO(b'x') if member.size else None)
        return archive

    def test_archive_paths_types_and_completeness(self):
        # Test parser defenses beyond the mandatory pinned hash check.
        cases = [([('../escape', tarfile.REGTYPE)], 'unsafe'),
                 ([(bsd.PREFIX + 'COPYRIGHT', tarfile.SYMTYPE)], 'non-regular'),
                 ([(bsd.PREFIX + 'COPYRIGHT', tarfile.REGTYPE)] * 2, 'duplicate'),
                 ([(bsd.PREFIX + 'COPYRIGHT', tarfile.REGTYPE)], 'missing selected')]
        for members, diagnostic in cases:
            with self.subTest(diagnostic=diagnostic):
                archive = self.synthetic_archive(members)
                checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
                with patch.object(bsd, 'ARCHIVE_SHA256', checksum):
                    with self.assertRaisesRegex(ValueError, diagnostic):
                        bsd.archive_index(archive)

    def test_historical_symlinks_are_inert_and_all_members_are_retained(self):
        members = [(bsd.PREFIX + 'COPYRIGHT', tarfile.REGTYPE),
                   (bsd.PREFIX + 'not-in-selection', tarfile.REGTYPE),
                   (bsd.PREFIX + 'absolute-link', tarfile.SYMTYPE),
                   (bsd.PREFIX + 'empty-dir', tarfile.DIRTYPE)]
        archive = self.synthetic_archive(members)
        checksum = bsd.digest(archive.read_bytes())
        with patch.object(bsd, 'ARCHIVE_SHA256', checksum), patch.object(bsd, 'SELECTED', ('COPYRIGHT',)):
            index, contents = bsd.archive_index(archive)
        self.assertEqual(set(contents), {'COPYRIGHT', 'not-in-selection', 'absolute-link'})
        self.assertEqual(contents['absolute-link'], b'/usr/obj/example')
        record = next(r for r in index['files'] if r['path'] == 'absolute-link')
        self.assertEqual((record['type'], record['link_target']), ('symlink', '/usr/obj/example'))
        self.assertEqual(index['directories'][0]['path'], 'empty-dir')
        # Importing into an empty root must create an ordinary file, never a link.
        fresh = self.root / 'fresh'
        bsd.import_reference(index, contents, fresh)
        link = fresh / bsd.REFERENCE / 'source/absolute-link'
        self.assertFalse(link.is_symlink())
        self.assertEqual(link.read_bytes(), b'/usr/obj/example')
        self.assertTrue((fresh / bsd.REFERENCE / 'source/empty-dir').is_dir())

    def test_archive_rejects_file_or_link_ancestors(self):
        archive = self.synthetic_archive([(bsd.PREFIX + 'COPYRIGHT', tarfile.REGTYPE),
                                          (bsd.PREFIX + 'parent', tarfile.SYMTYPE),
                                          (bsd.PREFIX + 'parent/child', tarfile.REGTYPE)])
        with patch.object(bsd, 'ARCHIVE_SHA256', bsd.digest(archive.read_bytes())), \
                patch.object(bsd, 'SELECTED', ('COPYRIGHT',)):
            with self.assertRaisesRegex(ValueError, 'used as parent'):
                bsd.archive_index(archive)


if __name__ == '__main__':
    unittest.main()
