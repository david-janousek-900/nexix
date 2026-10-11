#include <stdlib.h>
#include <signal.h>
/* V7 abort.s executes IOT and returns zero if control resumes. Do not reset
 * the disposition, flush stdio, or force exit after a returning handler. */
int abort(void) { return raise(SIGIOT); }
