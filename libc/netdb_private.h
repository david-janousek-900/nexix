#ifndef NEXIX_NETDB_PRIVATE_H
#define NEXIX_NETDB_PRIVATE_H
#include <stdio.h>
int _nx_dbline(FILE *, char *, size_t);
unsigned _nx_dbwords(char *, char **, unsigned);
#endif
