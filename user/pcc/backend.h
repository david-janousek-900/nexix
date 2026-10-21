#ifndef NX_PCC_BACKEND_H
#define NX_PCC_BACKEND_H
#include "mfile1"
#include "../mip/templates.h"
#include "../mip/pass.h"
void pcc_emit_expression(const struct mip_arena *,uint32_t);
void pcc_emit_initializer(const struct mip_arena *,uint32_t,unsigned bits);
void pcc_note_frame(unsigned bytes);
unsigned pcc_frame_size(void);
int pcc_output_begin(void);
int pcc_output_end(void);
int pcc_output_tree(const struct mip_arena *,uint32_t,unsigned);
int pcc_read_pass(FILE *);
int pcc_target_printf(const char *,...);
int pcc_target_puts(const char *);
int pcc_target_putchar(int);
/* All target text uses the same sink in one-pass and split-pass builds. */
#define printf pcc_target_printf
#define puts pcc_target_puts
#define putchar pcc_target_putchar
#endif
