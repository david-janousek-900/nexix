#ifndef NX_MIP_DIAGNOSTICS_H
#define NX_MIP_DIAGNOSTICS_H
#include <stdio.h>
struct mip_diagnostics {
    FILE *stream;
    const char *filename;
    unsigned errors, warnings, limit;
    int fatal;
};
void mip_diagnostics_init(struct mip_diagnostics *, FILE *, const char *, unsigned limit);
/* warning=0: error, 1: warning, 2: fatal internal error. Never exits a process.
 * Returns -1 on fatal, limit or output failure; callers stop their pass. */
int mip_diagnostic(struct mip_diagnostics *, unsigned line, unsigned warning, const char *, ...);
#endif
