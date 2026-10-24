#ifndef NEXIX_NETDB_PRIVATE_H
#define NEXIX_NETDB_PRIVATE_H
#include <stdio.h>
int _nx_dbline(FILE *, char *, size_t);
unsigned _nx_dbwords(char *, char **, unsigned);
struct hostent;
void _sethtent(int);
void _endhtent(void);
struct hostent *_gethtent(void);
struct hostent *_gethtbyname(const char *);
struct hostent *_gethtbyaddr(const void *, int, int);
struct hostent *_nx_host_answer(const unsigned char *, int, int);
int _nx_dns_equal(const char *, const char *);
#endif
