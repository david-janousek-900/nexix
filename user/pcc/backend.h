#ifndef NX_PCC_BACKEND_H
#define NX_PCC_BACKEND_H
#include "mfile1"
#include "../mip/templates.h"
void pcc_emit_expression(const struct mip_arena *,uint32_t);
void pcc_emit_initializer(const struct mip_arena *,uint32_t,unsigned bits);
void pcc_note_frame(unsigned bytes);
unsigned pcc_frame_size(void);
#endif
