"""Instrument the production BRE compiler, including its bytecode redzones."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='cc')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='nx-expr-bounds-') as temp:
        binary = str(Path(temp)/'expr')
        flags = ['-std=gnu89', '-funsigned-char', '-fno-builtin', '-fno-pie', '-no-pie',
                 '-Wno-implicit-int', '-Wno-implicit-function-declaration',
                 '-Wno-return-type', '-Wno-int-conversion', '-Wno-incompatible-pointer-types',
                 '-fsanitize=address,undefined', '-g']
        subprocess.run(shlex.split(args.cc)+flags+[str(root/'user/v7/cmd/expr.c'), '-o', binary], check=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        cases = [('abc', '.'*255+'a'), ('abc', r'\('), ('a'*200, r'\(a*\)')]
        for subject, pattern in cases:
            result = subprocess.run([binary, subject, ':', pattern], capture_output=True,
                                    text=True, env=environment, timeout=5)
            actual = (result.returncode, result.stdout, result.stderr)
            if actual != (2, '', 'RE error\n'):
                raise AssertionError(f'pattern length {len(pattern)}: {actual!r}')
    print('PASS: expr ASan/UBSan bytecode capacity and malformed/oversized captures')


if __name__ == '__main__':
    main()
