#ifndef NX_ABI_TEST_H
#define NX_ABI_TEST_H
struct abi_pair { long integer; double real; };
struct abi_large { long a, b, c; };
struct abi_pair abi_mixed(struct abi_pair);
struct abi_large abi_memory(long);
long double abi_extended(long double);
double abi_variadic(int, ...);
long abi_inspect(long, ...);
#endif
