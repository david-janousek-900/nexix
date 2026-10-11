#ifndef NEXIX_LIBC_STDIO_H
#define NEXIX_LIBC_STDIO_H
#include <stddef.h>
#include <nexix/file.h>
#include <stdarg.h>
#define EOF (-1)
#define BUFSIZ 512
#define SEEK_SET NX_SEEK_SET
#define SEEK_CUR NX_SEEK_CUR
#define SEEK_END NX_SEEK_END
typedef struct nx_stream {
    int fd, used, error, eof, readable, writable, pushback, pushed;
    int buffering, owned, direction, append;
    unsigned char *buffer;
    size_t position, length;
} FILE;
extern FILE *stdin, *stdout, *stderr;
FILE *fopen(const char *, const char *);
FILE *fdopen(int, const char *);
FILE *freopen(const char *, const char *, FILE *);
int ungetc(int, FILE *);
int getw(FILE *);
int putw(int, FILE *);
int fclose(FILE *);
int fflush(FILE *);
int fileno(FILE *);
void setbuf(FILE *, char *);
int fgetc(FILE *);
int getc(FILE *);
int getchar(void);
int fputc(int, FILE *);
int putc(int, FILE *);
int putchar(int);
int fputs(const char *, FILE *);
int puts(const char *);
char *fgets(char *, int, FILE *);
char *gets(char *);
size_t fread(void *, size_t, size_t, FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
int fseek(FILE *, long, int);
long ftell(FILE *);
void rewind(FILE *);
int feof(FILE *);
int ferror(FILE *);
void clearerr(FILE *);
int vfprintf(FILE *, const char *, va_list);
int fprintf(FILE *, const char *, ...);
int printf(const char *, ...);
int vsnprintf(char *, size_t, const char *, va_list);
int snprintf(char *, size_t, const char *, ...);
int sprintf(char *, const char *, ...);
void perror(const char *);
int scanf(const char *, ...);
int fscanf(FILE *, const char *, ...);
int sscanf(const char *, const char *, ...);
int vfscanf(FILE *, const char *, va_list);
int vscanf(const char *, va_list);
int vsscanf(const char *, const char *, va_list);
/* V7 exported error messages; check bounds before indexing. */
extern char *sys_errlist[];
extern int sys_nerr;

#endif
