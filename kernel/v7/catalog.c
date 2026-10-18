#include "userland.h"
extern const uint8_t _binary_build_exec_test_elf_start[], _binary_build_exec_test_elf_end[];
extern const uint8_t _binary_build_libc_test_elf_start[], _binary_build_libc_test_elf_end[];
extern const uint8_t _binary_build_files_test_elf_start[], _binary_build_files_test_elf_end[];
extern const uint8_t _binary_build_tty_test_elf_start[], _binary_build_tty_test_elf_end[];
extern const uint8_t _binary_build_signals_test_elf_start[], _binary_build_signals_test_elf_end[];
extern const uint8_t _binary_build_v7_test_elf_start[], _binary_build_v7_test_elf_end[];
#include "install_catalog.inc"
#define DECLARE(path,symbol) extern const uint8_t symbol##_start[],symbol##_end[];
NX_INSTALL_PROGRAMS(DECLARE)
#undef DECLARE
static const struct embedded_program catalog[] = {
    {"/bin/exec-test", _binary_build_exec_test_elf_start, _binary_build_exec_test_elf_end},
    {"/bin/libc-test", _binary_build_libc_test_elf_start, _binary_build_libc_test_elf_end},
    {"/bin/files-test", _binary_build_files_test_elf_start, _binary_build_files_test_elf_end},
    {"/bin/tty-test", _binary_build_tty_test_elf_start, _binary_build_tty_test_elf_end},
    {"/bin/signals-test", _binary_build_signals_test_elf_start, _binary_build_signals_test_elf_end},
    {"/bin/v7-test", _binary_build_v7_test_elf_start, _binary_build_v7_test_elf_end},
#define PROGRAM(path,symbol) {path,symbol##_start,symbol##_end},
    NX_INSTALL_PROGRAMS(PROGRAM)
#undef PROGRAM
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
