#ifndef NEXIX_ABI_KLOG_H
#define NEXIX_ABI_KLOG_H
#define NX_KLOG_CAPACITY 16384U
/* NX_SYS_KLOG: root-only, non-destructive chronological UTF-8/ASCII byte snapshot.
 * NULL/0 queries size; short buffers return ERANGE without writes. No trailing NUL.
 * Retained messages use LF and [uptime.microseconds] prefixes. */
#endif
