"""Compare native error text with every preserved V7 table entry."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
def messages(path):
    body=re.search(r'sys_errlist\s*\[\s*\]\s*=\s*\{(.*?)\};',path.read_text(),re.S)
    assert body is not None,'missing exported error table'
    return re.findall(r'"([^"\n]*)"',body[1])
reference=messages(ROOT/'reference/v7/usr/src/libc/gen/errlst.c')
native=messages(ROOT/'libc/errors.c')
assert len(reference)==35
assert native[:35]==reference,'V7 messages reordered or changed'
assert native[35:]==['Function not implemented'],'ENOSYS extension drift'
constants=dict(re.findall(r'^#define NX_(\w+) (\d+)$',(ROOT/'include/nexix/errno.h').read_text(),re.M))
assert sorted(map(int,constants.values()))==list(range(1,36)),'errno numbering gaps or duplicates'
assert constants['ENOSYS']=='35'
print('PASS: all 35 original V7 messages preserved at their indexes, contiguous errno values and ENOSYS extension')
