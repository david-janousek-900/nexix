#ifndef NEXIX_LIBC_ELF_H
#define NEXIX_LIBC_ELF_H
#include <stdint.h>
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword, Elf64_Addr, Elf64_Off;
typedef int64_t Elf64_Sxword;
#define EI_NIDENT 16
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define ELFMAG "\177ELF"
#define SELFMAG 4
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define EM_X86_64 62
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_NOBITS 8
#define SHT_DYNSYM 11
#define SHF_WRITE 1
#define SHF_ALLOC 2
#define SHF_EXECINSTR 4
#define SHN_UNDEF 0
#define SHN_LORESERVE 0xff00
#define SHN_ABS 0xfff1
#define SHN_COMMON 0xfff2
#define SHN_XINDEX 0xffff
#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2
#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4
#define STT_COMMON 5
#define STT_TLS 6
#define ELF64_ST_BIND(i) ((unsigned)(i) >> 4)
#define ELF64_ST_TYPE(i) ((i) & 15)
#define ELF64_ST_INFO(b,t) (((b) << 4) | ((t) & 15))
typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half e_type, e_machine;
    Elf64_Word e_version;
    Elf64_Addr e_entry;
    Elf64_Off e_phoff, e_shoff;
    Elf64_Word e_flags;
    Elf64_Half e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;
typedef struct {
    Elf64_Word sh_name, sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr sh_addr;
    Elf64_Off sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word sh_link, sh_info;
    Elf64_Xword sh_addralign, sh_entsize;
} Elf64_Shdr;
typedef struct {
    Elf64_Word st_name;
    unsigned char st_info, st_other;
    Elf64_Half st_shndx;
    Elf64_Addr st_value;
    Elf64_Xword st_size;
} Elf64_Sym;
_Static_assert(sizeof(Elf64_Ehdr)==64, "ELF64 header");
_Static_assert(sizeof(Elf64_Shdr)==64, "ELF64 section");
_Static_assert(sizeof(Elf64_Sym)==24, "ELF64 symbol");
#endif
