.section .text
.global _start
.type _start, @function
_start:
    xor %ebp, %ebp
    cld
    mov (%rsp), %rdi
    lea 8(%rsp), %rsi
    lea 16(%rsp,%rdi,8), %rdx
    mov %rdx, environ(%rip)
    and $-16, %rsp
    call main
    mov %eax, %edi
    call exit
    ud2
.size _start, .-_start
.section .note.GNU-stack,"",@progbits
