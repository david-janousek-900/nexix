#include <nexix/syscall.h>
#include <nexix/errno.h>
.section .text
.global _start
_start:
    test $15, %rsp
    jnz fail
    cmpq $2, (%rsp)
    jne fail
    cmpq $0, 24(%rsp)
    jne fail
    mov 16(%rsp), %rsi
    cmpb $'v', (%rsi)
    je empty_environment
    mov 32(%rsp), %rsi
    cmpl $0x59584e, (%rsi) /* NXY followed by NUL */
    jne fail
    cmpq $0, 40(%rsp)
    jne fail
    cmpq $0, 48(%rsp)
    jne fail
    cmpq $0, 56(%rsp)
    jne fail
    mov 16(%rsp), %rsi
    cmpb $'r', (%rsi)
    je replaced
    cmpb $'s', (%rsi)
    jne fail
    mov $NX_SYS_GETPID, %eax
    int $0x80
    mov %rax, saved_pid(%rip)
    mov $NX_SYS_UMASK, %eax
    mov $077, %edi
    int $0x80
    mov $NX_SYS_EXECVE, %eax
    xor %edi, %edi
    xor %esi, %esi
    xor %edx, %edx
    int $0x80
    cmp $-NX_EFAULT, %rax
    jne fail
    mov $NX_SYS_EXECVE, %eax
    lea missing(%rip), %rdi
    int $0x80
    cmp $-NX_ENOENT, %rax
    jne fail
    mov $NX_SYS_EXECVE, %eax
    lea path(%rip), %rdi
    mov $1, %esi
    int $0x80
    cmp $-NX_EFAULT, %rax
    jne fail
    mov $NX_SYS_EXECVE, %eax
    lea path(%rip), %rdi
    xor %esi, %esi
    mov $1, %edx
    int $0x80
    cmp $-NX_EFAULT, %rax
    jne fail
    xor %edx, %edx
    mov $NX_SYS_EXECVE, %eax
    lea path(%rip), %rdi
    lea large_vector(%rip), %rsi
    int $0x80
    cmp $-NX_E2BIG, %rax
    jne fail
    lea large_arg(%rip), %rdi
    mov $8192, %ecx
    mov $'a', %al
    rep stosb
    movb $0, -1(%rdi)
    mov $NX_SYS_EXECVE, %eax
    lea path(%rip), %rdi
    lea large_strings(%rip), %rsi
    int $0x80
    cmp $-NX_E2BIG, %rax
    jne fail
    /* Fork, replace the child, and wait from the still-running old image. */
    mov $NX_SYS_FORK, %eax
    int $0x80
    test %rax, %rax
    js fail
    jz child
    mov %rax, %r12
    mov $NX_SYS_WAIT, %eax
    lea status(%rip), %rdi
    int $0x80
    cmp %r12, %rax
    jne fail
    cmpl $0x2a00, status(%rip)
    jne fail
    mov $NX_SYS_GETPID, %eax
    int $0x80
    cmp saved_pid(%rip), %rax
    jne fail
    mov $NX_SYS_FORK, %eax
    int $0x80
    test %rax, %rax
    js fail
    jz execv_child
    mov %rax, %r12
    mov $NX_SYS_WAIT, %eax
    lea status(%rip), %rdi
    int $0x80
    cmp %r12, %rax
    jne fail
    cmpl $0x2a00, status(%rip)
    jne fail
    xor %edi, %edi
    jmp exit
execv_child:
    mov $NX_SYS_EXEC, %eax
    lea path(%rip), %rdi
    lea execv_argv(%rip), %rsi
    mov $1, %edx /* Two-argument exec must ignore this register. */
    int $0x80
    jmp fail
empty_environment:
    cmpq $0, 32(%rsp)
    jne fail
    cmpq $0, 40(%rsp)
    jne fail
    cmpq $0, 48(%rsp)
    jne fail
    jmp replaced
child:
    /* Dirty BSS must be zero again after exec. */
    movq $99, scratch(%rip)
    mov $NX_SYS_EXECVE, %eax
    lea path(%rip), %rdi
    lea replacement_argv(%rip), %rsi
    lea environment(%rip), %rdx
    std
    int $0x80
    jmp fail
replaced:
    pushfq
    pop %rax
    test $0x400, %eax
    jnz fail
    cmpq $0, scratch(%rip)
    jne fail
    cmpq $0, saved_pid(%rip)
    jne fail
    lea scratch(%rip), %rdi
    mov $8192, %ecx
    xor %eax, %eax
    repe scasb
    jne fail
    lea large_arg(%rip), %rdi
    mov $8192, %ecx
    repe scasb
    jne fail
    mov $NX_SYS_UMASK, %eax
    mov $022, %edi
    int $0x80
    cmp $077, %rax
    jne fail
    mov $42, %edi
    jmp exit
fail:
    cld
    mov $99, %edi
exit:
    mov $NX_SYS_EXIT, %eax
    int $0x80
    ud2
.section .rodata
path: .asciz "/bin/exec-test"
missing: .asciz "/missing"
replace: .asciz "replace"
execv: .asciz "vexec"
env: .asciz "NXY"
.section .data
replacement_argv: .quad path, replace, 0
execv_argv: .quad path, execv, 0
environment: .quad env, 0
large_strings: .quad path, large_arg, 0
large_vector:
.rept 65
.quad path
.endr
.quad 0
.section .bss
.balign 8
saved_pid: .skip 8
scratch: .skip 8192
status: .skip 4
large_arg: .skip 8192
.section .note.GNU-stack,"",@progbits
