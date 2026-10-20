"""Local QMP access for inspecting a live Nexix guest independently of its tty."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess


class Monitor:
    def __init__(self, path):
        self.socket = socket.socket(socket.AF_UNIX)
        self.socket.settimeout(5)
        self.socket.connect(str(path))
        self.stream = self.socket.makefile('rwb', buffering=0)
        json.loads(self.stream.readline())
        self.call('qmp_capabilities')
        self.socket.settimeout(60)

    def call(self, name, **arguments):
        self.stream.write((json.dumps({'execute': name, 'arguments': arguments}) + '\n').encode())
        while True:
            reply = json.loads(self.stream.readline())
            if 'error' in reply:
                raise RuntimeError(reply['error'])
            if 'return' in reply:
                return reply['return']

    def close(self):
        self.stream.close()
        self.socket.close()


def prepare(directory):
    directory = Path(directory).resolve()
    if directory.exists():
        # Never rename the sockets of a running guest, even if its pidfile is
        # absent (for example while QEMU is still starting).
        probe = socket.socket(socket.AF_UNIX)
        try:
            probe.connect(str(directory / 'qmp.sock'))
        except (FileNotFoundError, ConnectionRefusedError):
            pass
        else:
            raise RuntimeError(f'a debug guest is already listening in {directory}')
        finally:
            probe.close()
        pidfile = directory / 'qemu.pid'
        if pidfile.exists():
            try:
                os.kill(int(pidfile.read_text()), 0)
            except ProcessLookupError:
                pass
            else:
                raise RuntimeError(f'debug QEMU pid is still alive: {pidfile.read_text().strip()}')
        if any(directory.iterdir()):
            archive = directory.with_name(directory.name + '-' + stamp())
            directory.rename(archive)
            print(f'Previous debug session: {archive}')
    directory.mkdir(parents=True, exist_ok=True)


def stamp():
    return datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')


def capture(directory):
    directory = Path(directory).resolve()
    monitor = Monitor(directory / 'qmp.sock')
    destination = directory / ('capture-' + stamp())
    destination.mkdir()
    state = monitor.call('query-status')
    running = state['running']
    try:
        if running:
            monitor.call('stop')
        report = {'before': state, 'stopped': monitor.call('query-status'),
                  'cpus': monitor.call('query-cpus-fast')}
        for command in ('info registers', 'info pic', 'info irq', 'x /16i $pc'):
            report[command] = monitor.call('human-monitor-command', **{'command-line': command})
        (destination / 'state.json').write_text(json.dumps(report, indent=2) + '\n')
        shutil.copyfile('build/nexix.elf', destination / 'nexix.elf')
        revision = subprocess.run(['git', 'rev-parse', 'HEAD'], capture_output=True, text=True, check=True).stdout
        (destination / 'revision.txt').write_text(revision)
        monitor.call('dump-guest-memory', paging=False, protocol='file:' + str(destination / 'memory.elf'))
        if (directory / 'qemu.log').exists():
            shutil.copyfile(directory / 'qemu.log', destination / 'qemu.log')
    finally:
        try:
            if running:
                monitor.call('cont')
        finally:
            monitor.close()
    print(f'Capture: {destination}')
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['prepare', 'capture'])
    parser.add_argument('directory', nargs='?', default='build/debug')
    args = parser.parse_args()
    try:
        globals()[args.action](args.directory)
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'Debug access failed: {error}\n')


if __name__ == '__main__':
    main()
