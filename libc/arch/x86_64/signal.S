#include <nexix/syscall.h>
.text
.global __nx_signal_return
.type __nx_signal_return,@function
__nx_signal_return:
    mov %rsp, %rdi
    mov $NX_SYS_SIGRETURN, %eax
    int $0x80
    ud2
.size __nx_signal_return,.-__nx_signal_return
.section .note.GNU-stack,"",@progbits
