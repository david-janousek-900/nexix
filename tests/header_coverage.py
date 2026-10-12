"""Compile every public header in isolation without falling back to host libc."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]


def audit_errors(audit, root, readme):
    errors=[]
    if audit.get('schema_version') != 2:
        return ['unsupported header schema']
    records=audit.get('headers', [])
    sources=[r['source'] for r in records]
    actual={str(p.relative_to(root)) for p in (root/'reference/v7/usr/include').rglob('*') if p.is_file()}
    if len(sources)!=len(set(sources)) or set(sources)!=actual:
        errors.append('header coverage omission/duplicate/unknown input')
    tasks={int(n) for n in re.findall(r'^\| (\d+) \|',readme,re.M)}
    for record in records:
        if type(record.get('task')) is not int or record['task'] not in tasks:
            errors.append('invalid header owner')
        status=record.get('status')
        if status not in {'native_contract','native_disk_contract','private_kernel','deferred_consumer'}:
            errors.append('invalid header disposition')
        target=record.get('native')
        if status in {'native_contract','native_disk_contract'}:
            if not isinstance(target,str) or Path(target).is_absolute() or '..' in Path(target).parts or not (root/target).is_file():
                errors.append('invalid native header path')
        elif target is not None:
            errors.append('deferred header cannot claim native implementation')
    return errors


def main():
    args=argparse.ArgumentParser()
    args.add_argument('--cc',default='gcc')
    cc=args.parse_args().cc
    audit=json.loads((ROOT/'docs/header-coverage.json').read_text())
    readme=(ROOT/'README.md').read_text()
    assert not audit_errors(audit,ROOT,readme),audit_errors(audit,ROOT,readme)
    # Deliberately corrupt coverage rather than testing only a valid fixture.
    import copy
    for mutation in ('omit','duplicate','owner','path','status','legacy_schema','legacy_owner'):
        bad=copy.deepcopy(audit)
        if mutation=='omit': bad['headers'].pop()
        elif mutation=='duplicate': bad['headers'].append(bad['headers'][0])
        elif mutation=='owner': bad['headers'][0]['task']=True
        elif mutation=='status': bad['headers'][0]['status']='implemented'
        elif mutation=='legacy_schema': bad['schema_version']=1
        elif mutation=='legacy_owner': bad['headers'][0]['step']=bad['headers'][0].pop('task')
        else: bad['headers'][0].update(status='native_contract',native='/etc/passwd')
        assert audit_errors(bad,ROOT,readme),mutation
    compiler_headers=subprocess.check_output([cc,'-print-file-name=include'],text=True).strip()
    headers=sorted(p.relative_to(ROOT/'libc/include') for p in (ROOT/'libc/include').rglob('*.h'))
    with tempfile.TemporaryDirectory(prefix='nexix-headers-') as folder:
        source=Path(folder)/'header.c'
        for standard in ('c11','gnu89'):
            for header in headers:
                source.write_text(f'#include <{header}>\n#include <{header}>\n')
                subprocess.run([cc,'-std='+standard,'-Werror','-Wall','-Wextra','-ffreestanding','-fno-builtin','-nostdinc','-isystem',compiler_headers,'-I'+str(ROOT/'libc/include'),'-I'+str(ROOT/'include'),'-fsyntax-only',str(source)],check=True)
    print(f'PASS: {len(audit["headers"])} V7 header inputs; {len(headers)} isolated public headers in C11/gnu89; 7 negative audit checks')


if __name__=='__main__': main()
