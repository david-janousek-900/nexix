#include <signal.h>
#include <unistd.h>
int main(void){raise(SIGABRT);_exit(99);}
