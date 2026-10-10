#include <nexix/syscall.h>
.text
.global signal_register_test
.type signal_register_test,@function
signal_register_test:
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15
    mov $NX_SYS_GETPID, %eax
    int $0x80
    mov %rax, %rdi
    mov $15, %esi
    mov $0x11223344, %edx
    mov $0x22334455, %ecx
    mov $0x33445566, %ebx
    mov $0x44556677, %ebp
    mov $0x55667788, %r8d
    mov $0x66778899, %r9d
    mov $0x778899aa, %r10d
    mov $0x12345678, %r11d
    mov $0x23456789, %r12d
    mov $0x3456789a, %r13d
    mov $0x456789ab, %r14d
    mov $0x56789abc, %r15d
    std
    stc
    mov $NX_SYS_KILL, %eax
    int $0x80
    pushfq
    pop %rdi
    cld
    and $0x401, %edi
    cmp $0x401, %edi
    jne 1f
    test %rax, %rax
    jne 1f
    cmp $15, %esi
    jne 1f
    cmp $0x11223344, %rdx
    jne 1f
    cmp $0x22334455, %rcx
    jne 1f
    cmp $0x33445566, %rbx
    jne 1f
    cmp $0x44556677, %rbp
    jne 1f
    cmp $0x55667788, %r8
    jne 1f
    cmp $0x66778899, %r9
    jne 1f
    cmp $0x778899aa, %r10
    jne 1f
    cmp $0x12345678, %r11
    jne 1f
    cmp $0x23456789, %r12
    jne 1f
    cmp $0x3456789a, %r13
    jne 1f
    cmp $0x456789ab, %r14
    jne 1f
    cmp $0x56789abc, %r15
    jne 1f
    mov $1, %eax
    jmp 2f
1: xor %eax, %eax
2: pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
    ret
.size signal_register_test,.-signal_register_test
.section .note.GNU-stack,"",@progbits
