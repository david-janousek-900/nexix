.section .text
.global nx_syscall6
.type nx_syscall6, @function
nx_syscall6:
    /* SysV arguments: RDI number, RSI a1, RDX a2, RCX a3, R8 a4,
     * R9 a5, [RSP+8] a6. Kernel ABI has number in RAX and a4 in R10. */
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsi
    mov %rcx, %rdx
    mov %r8, %r10
    mov %r9, %r8
    mov 8(%rsp), %r9
    int $0x80
    ret
.size nx_syscall6, .-nx_syscall6
.section .note.GNU-stack,"",@progbits
