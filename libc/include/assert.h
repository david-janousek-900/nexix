/* V7 assertions terminate through exit(1), not abort. Re-inclusion after
 * changing NDEBUG updates the macros as in the original unguarded header. */
#include <stdio.h>
#include <stdlib.h>
#undef assert
#undef _assert
#ifdef NDEBUG
#define assert(expression) ((void)0)
#define _assert(expression) ((void)0)
#else
#define assert(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "Assertion failed: file %s, line %d\n", __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)
#define _assert(expression) assert(expression)
#endif
