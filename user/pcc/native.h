/* Explicit native-header source profile for the retained V7 frontend.
 * Feed this file to cpp before the application. Qualifiers/inline are erased
 * by this compatibility profile; loads/stores remain conservative and these
 * header hints do not change the native ABI. This is not a C11 language mode.
 */
#ifndef NX_PCC_NATIVE_PROFILE
#define NX_PCC_NATIVE_PROFILE
#define const
#define volatile
#define restrict
#define _Noreturn
#define inline
#define __inline__
#define __restrict
#define __restrict__
#define __builtin_offsetof(type,member) ((unsigned long)&((type *)0)->member)
#define _Static_assert(condition,message) extern char __nx_pcc_assert[(condition)?1:-1]
typedef struct __nx_pcc_va_state {
    unsigned gp_offset,fp_offset;
    void *overflow_arg_area,*reg_save_area;
} __nx_pcc_va_list[1];
#define __builtin_va_list __nx_pcc_va_list
#endif
