#include <nexix/syscall.h>
.section .text
.global libc_alignment_test
.type libc_alignment_test, @function
libc_alignment_test:
    mov %rsp, %rax
    and $15, %eax
    cmp $8, %eax
    sete %al
    movzbl %al, %eax
    ret
.size libc_alignment_test, .-libc_alignment_test
.global libc_register_test
.type libc_register_test, @function
libc_register_test:
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15
    sub $8, %rsp
    movq $0, (%rsp)
    movabs $0x1111222233334444, %rbx
    mov %rbx, %rbp
    mov %rbx, %r12
    mov %rbx, %r13
    mov %rbx, %r14
    mov %rbx, %r15
    mov $NX_SYS_GETPID, %edi
    xor %esi, %esi
    xor %edx, %edx
    xor %ecx, %ecx
    xor %r8d, %r8d
    xor %r9d, %r9d
    call nx_syscall6
    movabs $0x1111222233334444, %r11
    xor %eax, %eax
    cmp %r11, %rbx
    jne 1f
    cmp %r11, %rbp
    jne 1f
    cmp %r11, %r12
    jne 1f
    cmp %r11, %r13
    jne 1f
    cmp %r11, %r14
    jne 1f
    cmp %r11, %r15
    jne 1f
    mov $1, %eax
1:
    add $8, %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
    ret
.size libc_register_test, .-libc_register_test
.section .note.GNU-stack,"",@progbits
