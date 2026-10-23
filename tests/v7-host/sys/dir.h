/* Host-test stand-in for the native V7 directory record (see libc/include/sys/dir.h). */
#include <stdint.h>
#define DIRSIZ 14
struct direct { uint16_t d_ino; char d_name[DIRSIZ]; };
