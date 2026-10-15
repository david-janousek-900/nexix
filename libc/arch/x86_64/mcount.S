.section .text
.global mcount
.type mcount,@function
mcount:
    pushfq
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rbp
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    mov %rsp,%r11
    sub $528,%rsp
    and $-16,%rsp
    mov %r11,512(%rsp)
    fxsave64 (%rsp)
    cld
    mov 128(%r11),%rdi
    call __nx_mcount
    fxrstor64 (%rsp)
    mov 512(%rsp),%rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rbp
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    popfq
    ret
.size mcount,.-mcount
.section .note.GNU-stack,"",@progbits
