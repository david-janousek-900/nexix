"""Compare all V7 salt permutations with the independently installed libcrypt."""
import ctypes
import ctypes.util
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
native=ctypes.CDLL(str(ROOT/'build/crypt-oracle.so'))
native.nx_test_crypt.argtypes=[ctypes.c_char_p,ctypes.c_char_p]
native.nx_test_crypt.restype=ctypes.c_char_p
library=ctypes.util.find_library('crypt')
if library is None:
    print('SKIP: optional libcrypt differential oracle unavailable; fixed DES/hash vectors still run')
else:
    oracle=ctypes.CDLL(library)
    oracle.crypt.argtypes=[ctypes.c_char_p,ctypes.c_char_p]
    oracle.crypt.restype=ctypes.c_char_p
    alphabet=b'./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'
    for a in alphabet:
        for b in alphabet:
            salt=bytes([a,b])
            expected=oracle.crypt(b'password',salt)
            assert expected and not expected.startswith(b'*'),'host oracle lacks traditional DES crypt'
            assert native.nx_test_crypt(b'password',salt)==expected,repr(salt)
    print('PASS: all 4096 two-character DES salt permutations match independent host libcrypt')
