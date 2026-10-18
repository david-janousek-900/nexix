"""Independent bootstrap/full build graphs and non-clobber disk publication."""
import concurrent.futures
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from seed_disk import seed_disk
from root_separation_fixture import fixture
from install_manifest import MANIFEST, BOOTSTRAP, BOOTSTRAP_CONFIG, load, validate_profiles
from mk_rootfs import build


class Separation(unittest.TestCase):
    def test_full_payload_does_not_enter_kernel(self):
        with tempfile.TemporaryDirectory(prefix='nexix-separation-') as temp:fixture(temp)

    def test_bootstrap_contract(self):
        entries=load(BOOTSTRAP)
        self.assertEqual(sum(e.get('format')=='elf' for e in entries),11)
        self.assertTrue(all(e.get('bootstrap') for e in entries if e.get('format')=='elf'))
        data=build(1024,Path('build'),BOOTSTRAP_CONFIG,BOOTSTRAP,inode_blocks=12)
        self.assertEqual(len(data),512*1024)
        self.assertEqual(data,Path('build/root.img').read_bytes())
        self.assertFalse(any(e['path'].startswith('/usr/full-') for e in entries))

    def test_profile_drift_rejected(self):
        full=load(MANIFEST);bootstrap=load(BOOTSTRAP);validate_profiles(full,bootstrap)
        full[0]['bootstrap']=True
        with self.assertRaises(ValueError):validate_profiles(full,bootstrap)
        full=load(MANIFEST);bootstrap=[e for e in bootstrap if e['path']!='/bin/sh']
        with self.assertRaises(ValueError):validate_profiles(full,bootstrap)
        bootstrap=load(BOOTSTRAP);full=[e for e in full if e['path']!='/etc/init']
        with self.assertRaises(ValueError):validate_profiles(full,bootstrap)

    def test_existing_disk_needs_no_seed(self):
        with tempfile.TemporaryDirectory() as temp:
            base=Path(temp);disk=base/'disk';disk.write_bytes(b'user data')
            self.assertFalse(seed_disk(base/'absent',disk));self.assertEqual(disk.read_bytes(),b'user data')
            link=base/'link';link.symlink_to(disk)
            self.assertFalse(seed_disk(base/'absent',link));self.assertTrue(link.is_symlink())

    def test_failed_copy_and_publication(self):
        with tempfile.TemporaryDirectory() as temp:
            base=Path(temp);seed=base/'seed';seed.write_bytes(b'new disk');disk=base/'disk'
            for function in ('seed_disk.shutil.copyfileobj','seed_disk.os.link'):
                with patch(function,side_effect=OSError('fault')):
                    with self.assertRaises(OSError):seed_disk(seed,disk)
                self.assertFalse(disk.exists());self.assertEqual(list(base.iterdir()),[seed])

    def test_competing_creators_do_not_replace(self):
        with tempfile.TemporaryDirectory() as temp:
            base=Path(temp);seed=base/'seed';seed.write_bytes(b'complete seed'*8192);disk=base/'disk'
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
                results=list(pool.map(lambda _:seed_disk(seed,disk),range(4)))
            self.assertEqual(sum(results),1);self.assertEqual(seed.read_bytes(),disk.read_bytes())
            self.assertEqual(set(base.iterdir()),{seed,disk})


if __name__=='__main__':unittest.main()
