#ifndef NX_AS_ENCODE_H
#define NX_AS_ENCODE_H
#include "as.h"
struct as_fixup {unsigned offset,width,type,pc_bias;struct as_expr expression;};
struct as_code {unsigned char bytes[32];unsigned length,fixup_count;struct as_fixup fixups[2];};
int as_encode(const struct as_record *,struct as_code *);
#endif
