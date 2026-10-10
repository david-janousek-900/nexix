.section .text
.code64
.global arch_context_switch
.type arch_context_switch, @function
arch_context_switch:
    # Interrupts are disabled; C owns CR3, current process, and TSS.RSP0.
    mov %rbx, 0(%rdi)
    mov %rbp, 8(%rdi)
    mov %r12, 16(%rdi)
    mov %r13, 24(%rdi)
    mov %r14, 32(%rdi)
    mov %r15, 40(%rdi)
    lea 8(%rsp), %rax
    mov %rax, 48(%rdi)
    mov (%rsp), %rax
    mov %rax, 56(%rdi)
    mov 0(%rsi), %rbx
    mov 8(%rsi), %rbp
    mov 16(%rsi), %r12
    mov 24(%rsi), %r13
    mov 32(%rsi), %r14
    mov 40(%rsi), %r15
    mov 48(%rsi), %rsp
    jmp *56(%rsi)

.global arch_process_enter
.type arch_process_enter, @function
arch_process_enter:
    mov %rdi, %rsp
    jmp arch_interrupt_return

# ABI register probe: verifies all callee-saved registers through yield.
.global arch_context_probe
.type arch_context_probe, @function
arch_context_probe:
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15
    sub $8, %rsp
    mov $0x1111, %ebx
    mov $0x2222, %ebp
    mov $0x3333, %r12d
    mov $0x4444, %r13d
    mov $0x5555, %r14d
    mov $0x6666, %r15d
    call proc_yield
    xor %eax, %eax
    cmp $0x1111, %rbx
    jne 1f
    cmp $0x2222, %rbp
    jne 1f
    cmp $0x3333, %r12
    jne 1f
    cmp $0x4444, %r13
    jne 1f
    cmp $0x5555, %r14
    jne 1f
    cmp $0x6666, %r15
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
.section .note.GNU-stack,"",@progbits
