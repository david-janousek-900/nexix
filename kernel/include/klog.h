#ifndef NEXIX_KLOG_H
#define NEXIX_KLOG_H
#include <stdint.h>
#include <stddef.h>
int64_t klog_request(uintptr_t buffer,size_t capacity);
#endif
