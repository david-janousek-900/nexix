/* Native counterpart of V7 files.c's archive-member / entry lookup.
 * Read the installed portable ar container and defined ELF64 global symbols;
 * never interpret PDP-11 a.out or host-sized archive structs. */
#include "defs"
#include <elf.h>
#include <limits.h>
#include <sys/stat.h>

static unsigned long decimal(const char *p, size_t n)
{
    unsigned long value=0;
    size_t i=0;
    while (i<n && p[i]==' ') i++;
    for (; i<n && p[i]!=' '; i++) {
        if (p[i]<'0' || p[i]>'9' || value>(LONG_MAX-(p[i]-'0'))/10)
            fatal("bad archive number");
        value=value*10+p[i]-'0';
    }
    for (; i<n; i++) if (p[i]!=' ') fatal("bad archive number");
    return value;
}

static void record(FILE *file, long base, uint64_t size, uint64_t offset,
                   void *data, size_t count)
{
    if (offset>size || count>size-offset || offset>(uint64_t)(LONG_MAX-base) ||
        fseek(file,base+(long)offset,SEEK_SET) || fread(data,1,count,file)!=count)
        fatal("truncated archive or object");
}

static int symbol(FILE *file, long base, uint64_t size, const char *wanted)
{
    Elf64_Ehdr header;
    Elf64_Shdr section, strings;
    Elf64_Sym entry;
    uint64_t i,j,k,sections;
    unsigned char magic[4];
    char c;
    if (size<4) return 0;
    record(file,base,size,0,magic,4);
    if (memcmp(magic,ELFMAG,4)) return 0; /* non-object archive member */
    record(file,base,size,0,&header,sizeof header);
    if (header.e_ident[EI_CLASS]!=ELFCLASS64 || header.e_ident[EI_DATA]!=ELFDATA2LSB ||
        header.e_machine!=EM_X86_64 || header.e_shentsize!=sizeof section ||
        header.e_shoff>size || header.e_shnum>(size-header.e_shoff)/sizeof section)
        fatal("unsupported or malformed archive ELF64");
    sections=header.e_shoff;
    for (i=0;i<header.e_shnum;i++) {
        record(file,base,size,sections+i*sizeof section,&section,sizeof section);
        if (section.sh_type!=SHT_SYMTAB) continue;
        if (section.sh_entsize!=sizeof entry || section.sh_size%sizeof entry ||
            section.sh_offset>size || section.sh_size>size-section.sh_offset ||
            section.sh_link>=header.e_shnum) fatal("bad archive symbol table");
        record(file,base,size,sections+section.sh_link*sizeof strings,&strings,sizeof strings);
        if (strings.sh_type!=SHT_STRTAB || strings.sh_offset>size || strings.sh_size>size-strings.sh_offset)
            fatal("bad archive string table");
        for (j=0;j<section.sh_size/sizeof entry;j++) {
            record(file,base,size,section.sh_offset+j*sizeof entry,&entry,sizeof entry);
            if ((ELF64_ST_BIND(entry.st_info)!=STB_GLOBAL && ELF64_ST_BIND(entry.st_info)!=STB_WEAK) ||
                entry.st_shndx==SHN_UNDEF) continue;
            if (entry.st_name>=strings.sh_size) fatal("bad archive symbol name");
            for (k=0;;k++) {
                if (k>=strings.sh_size-entry.st_name) fatal("unterminated archive symbol name");
                record(file,base,size,strings.sh_offset+entry.st_name+k,&c,1);
                if (c!=wanted[k]) break;
                if (!c) return 1;
            }
        }
    }
    return 0;
}

TIMETYPE lookarch(char *filename)
{
    FILE *file;
    struct stat info;
    char archive[OUTMAX], wanted[OUTMAX], name[OUTMAX], header[60], magic[8];
    char *open=strchr(filename,'('), *end, *longnames=NULL;
    size_t prefix, length, namebytes, tablebytes=0;
    uint64_t pos=8, member_size, payload, extended, index;
    unsigned long date;
    int entry;
    TIMETYPE found=0;
    if (!open) fatal("bad archive target");
    prefix=(size_t)(open-filename);
    if (prefix>=sizeof archive) fatal("bad archive target");
    memcpy(archive,filename,prefix); archive[prefix]=0;
    open++; entry=*open=='('; if (entry) open++;
    end=strchr(open,')');
    if (!end || (entry ? strcmp(end,"))") : strcmp(end,")"))) fatal("bad archive target");
    length=(size_t)(end-open);
    if (!length || length>=sizeof wanted) fatal("bad archive member name");
    memcpy(wanted,open,length); wanted[length]=0;
    if (stat(archive,&info)<0) return 0;
    file=fopen(archive,"r"); if (!file) fatal1("cannot open %s",archive);
    if (info.st_size<8 || fread(magic,1,8,file)!=8 || memcmp(magic,"!<arch>\n",8))
        fatal1("%s is not a native archive",archive);
    while (pos<(uint64_t)info.st_size) {
        record(file,0,info.st_size,pos,header,sizeof header);
        if (memcmp(header+58,"`\n",2)) fatal("bad archive header");
        member_size=decimal(header+48,10); date=decimal(header+16,12);
        payload=pos+60;
        if (member_size>(uint64_t)info.st_size-payload) fatal("truncated archive member");
        pos=payload+member_size+(member_size&1);
        if (pos>(uint64_t)info.st_size) fatal("truncated archive alignment");
        extended=0;
        if (!memcmp(header,"//",2)) {
            free(longnames); tablebytes=member_size;
            longnames=malloc(tablebytes ? tablebytes : 1);
            if (!longnames) fatal("out of memory");
            record(file,0,info.st_size,payload,longnames,tablebytes);
            continue;
        }
        if (!memcmp(header,"#1/",3)) {
            extended=decimal(header+3,13);
            if (extended>=sizeof name || extended>member_size) fatal("bad extended archive name");
            record(file,0,info.st_size,payload,name,extended); name[extended]=0;
        } else if (header[0]=='/' && isdigit((unsigned char)header[1])) {
            index=decimal(header+1,15); namebytes=0;
            if (index>=tablebytes) fatal("bad archive long-name index");
            while (index<tablebytes && longnames[index]!='\n' && longnames[index]) {
                if (namebytes==sizeof name-1) fatal("archive name too long");
                name[namebytes++]=longnames[index++];
            }
            if (index==tablebytes) fatal("unterminated archive long name");
            if (namebytes && name[namebytes-1]=='/') namebytes--;
            name[namebytes]=0;
        } else {
            namebytes=16; while (namebytes && header[namebytes-1]==' ') namebytes--;
            if (namebytes && header[namebytes-1]=='/') namebytes--;
            memcpy(name,header,namebytes); name[namebytes]=0;
        }
        if (entry ? symbol(file,payload+extended,member_size-extended,wanted) : !strcmp(name,wanted)) {
            found=date ? date : (info.st_mtime ? info.st_mtime : 1); break;
        }
    }
    free(longnames); fclose(file); return found;
}
