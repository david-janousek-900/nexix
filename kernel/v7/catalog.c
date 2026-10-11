#include "userland.h"
extern const uint8_t _binary_build_exec_test_elf_start[], _binary_build_exec_test_elf_end[];
extern const uint8_t _binary_build_libc_test_elf_start[], _binary_build_libc_test_elf_end[];
extern const uint8_t _binary_build_files_test_elf_start[], _binary_build_files_test_elf_end[];
extern const uint8_t _binary_build_tty_test_elf_start[], _binary_build_tty_test_elf_end[];
extern const uint8_t _binary_build_signals_test_elf_start[], _binary_build_signals_test_elf_end[];
extern const uint8_t _binary_build_v7_test_elf_start[], _binary_build_v7_test_elf_end[];
extern const uint8_t _binary_build_v7_sh_elf_start[], _binary_build_v7_sh_elf_end[];
extern const uint8_t _binary_build_v7_init_elf_start[], _binary_build_v7_init_elf_end[];
extern const uint8_t _binary_build_v7_echo_elf_start[], _binary_build_v7_echo_elf_end[];
extern const uint8_t _binary_build_v7_cat_elf_start[], _binary_build_v7_cat_elf_end[];
extern const uint8_t _binary_build_v7_ls_elf_start[], _binary_build_v7_ls_elf_end[];
extern const uint8_t _binary_build_v7_pwd_elf_start[], _binary_build_v7_pwd_elf_end[];
extern const uint8_t _binary_build_v7_cp_elf_start[], _binary_build_v7_cp_elf_end[];
extern const uint8_t _binary_build_v7_ln_elf_start[], _binary_build_v7_ln_elf_end[];
extern const uint8_t _binary_build_v7_rm_elf_start[], _binary_build_v7_rm_elf_end[];
extern const uint8_t _binary_build_v7_date_elf_start[], _binary_build_v7_date_elf_end[];
extern const uint8_t _binary_build_v7_touch_elf_start[], _binary_build_v7_touch_elf_end[];
static const struct embedded_program catalog[] = {
    {"/bin/date", _binary_build_v7_date_elf_start, _binary_build_v7_date_elf_end},
    {"/bin/touch", _binary_build_v7_touch_elf_start, _binary_build_v7_touch_elf_end},
    {"/bin/exec-test", _binary_build_exec_test_elf_start, _binary_build_exec_test_elf_end},
    {"/bin/libc-test", _binary_build_libc_test_elf_start, _binary_build_libc_test_elf_end},
    {"/bin/files-test", _binary_build_files_test_elf_start, _binary_build_files_test_elf_end},
    {"/bin/tty-test", _binary_build_tty_test_elf_start, _binary_build_tty_test_elf_end},
    {"/bin/signals-test", _binary_build_signals_test_elf_start, _binary_build_signals_test_elf_end},
    {"/bin/v7-test", _binary_build_v7_test_elf_start, _binary_build_v7_test_elf_end},
    {"/bin/sh", _binary_build_v7_sh_elf_start, _binary_build_v7_sh_elf_end},
    {"/etc/init", _binary_build_v7_init_elf_start, _binary_build_v7_init_elf_end},
    {"/bin/echo", _binary_build_v7_echo_elf_start, _binary_build_v7_echo_elf_end},
    {"/bin/cat", _binary_build_v7_cat_elf_start, _binary_build_v7_cat_elf_end},
    {"/bin/ls", _binary_build_v7_ls_elf_start, _binary_build_v7_ls_elf_end},
    {"/bin/pwd", _binary_build_v7_pwd_elf_start, _binary_build_v7_pwd_elf_end},
    {"/bin/cp", _binary_build_v7_cp_elf_start, _binary_build_v7_cp_elf_end},
    {"/bin/ln", _binary_build_v7_ln_elf_start, _binary_build_v7_ln_elf_end},
    {"/bin/rm", _binary_build_v7_rm_elf_start, _binary_build_v7_rm_elf_end},
};
const struct embedded_program *userland_program(const char *name)
{
    for (unsigned i=0;i<sizeof(catalog)/sizeof(catalog[0]);++i) {
        const char *a=name,*b=catalog[i].path;
        while(*a&&*a==*b){++a;++b;}
        if(!*a&&!*b)return &catalog[i];
    }
    return NULL;
}
