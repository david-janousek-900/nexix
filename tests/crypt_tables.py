"""Ensure architecture adaptation preserves all supplied DES table values."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
def table(text,name):
    found=re.search(r'static\s+char\s+'+re.escape(name)+r'\s*(?:\[[^\]]*\])+\s*(?:=\s*)?\{',text)
    assert found is not None,name
    start=found.end();depth=1;end=start
    while depth:
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
        end+=1
    return re.findall(r'\b\d+\b',text[start:end-1])
reference=(ROOT/'reference/v7/usr/src/libc/gen/crypt.c').read_text()
native=(ROOT/'libc/crypt.c').read_text()
for name in ['IP','FP','PC1_C','PC1_D','shifts','PC2_C','PC2_D','e','S','P']:
    assert table(reference,name)==table(native,name),name+' changed'
print('PASS: all ten V7 DES permutation, shift, expansion and S-box tables preserved')
