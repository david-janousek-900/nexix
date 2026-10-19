"""Check host pipe preparation and the interactive connector protocol."""
import os
import json
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from serial_terminal import prepare


class Terminal(unittest.TestCase):
    def test_guest_configuration(self):
        plan=json.loads(Path('docs/native-install.json').read_text())['entries']
        nodes={e['path']:e for e in plan}
        self.assertEqual(nodes['/dev/ttyS0']['target'],'/dev/console')
        self.assertEqual(nodes['/dev/tty']['device'],0x200)
        line=nodes['/dev/ttyS1']
        self.assertEqual((line['type'],line['device'],line['mode'],line['uid'],line['gid']),('character',0x101,'0600',0,0))
        local=nodes['/dev/tty0']
        self.assertEqual((local['type'],local['device'],local['mode'],local['uid'],local['gid']),('character',0x102,'0600',0,0))
        self.assertEqual(Path('rootfs/etc/ttys').read_text().splitlines(),['19console','19ttyS1','19tty0'])
        self.assertEqual(Path('rootfs/bootstrap/etc/ttys').read_bytes(),b'')

    def test_preserve_non_fifo(self):
        with tempfile.TemporaryDirectory() as temp:
            prefix=Path(temp)/'serial';path=Path(str(prefix)+'.in');path.write_text('keep')
            with self.assertRaises(ValueError):prepare(prefix)
            self.assertEqual(path.read_text(),'keep')
            link=Path(temp)/'link.in';link.symlink_to(path)
            with self.assertRaises(ValueError):prepare(Path(temp)/'link')

    def test_connect_and_disconnect(self):
        with tempfile.TemporaryDirectory() as temp:
            prefix=Path(temp)/'serial';prepare(prefix);prepare(prefix)
            rx=os.open(str(prefix)+'.in',os.O_RDWR|os.O_NONBLOCK)
            tx=os.open(str(prefix)+'.out',os.O_RDWR|os.O_NONBLOCK)
            process=subprocess.Popen([sys.executable,'tools/serial_terminal.py',str(prefix)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            try:
                self.assertTrue(select.select([process.stderr],[],[],5)[0]);self.assertIn(b'connected',process.stderr.readline())
                os.write(tx,b'from guest\n')
                self.assertTrue(select.select([process.stdout],[],[],5)[0]);self.assertEqual(process.stdout.readline(),b'from guest\n')
                process.stdin.write(b'to guest\x1dignored');process.stdin.flush()
                self.assertEqual(process.wait(timeout=5),0)
                self.assertEqual(os.read(rx,100),b'to guest')
            finally:
                if process.poll() is None:process.kill();process.wait()
                process.stdin.close();process.stdout.close();process.stderr.close();os.close(rx);os.close(tx)


if __name__=='__main__':unittest.main()
