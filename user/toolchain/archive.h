#ifndef NX_ARCHIVE_H
#define NX_ARCHIVE_H
#include <stddef.h>
#include <stdint.h>
#define ARCHIVE_LIMIT (32UL*1024*1024)
#define ARCHIVE_MEMBERS 4096
struct archive_member {
    char *name;
    unsigned char *data;
    size_t size;
    uint64_t date,uid,gid,mode;
};
struct archive {
    struct archive_member *members;
    size_t count;
    int indexed,format;
};
void archive_free(struct archive *);
void archive_member_free(struct archive_member *);
int archive_load(const char *,struct archive *);
int archive_file(const char *,struct archive_member *);
int archive_replace(const char *,const unsigned char *,size_t);
int archive_write(const char *,const struct archive *,int);
int archive_replace_mode(const char *,const unsigned char *,size_t,unsigned);
int archive_safe_name(const char *);
#endif
