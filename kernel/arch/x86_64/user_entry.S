.section .text
.code64
.global arch_load_gdt
.type arch_load_gdt, @function
arch_load_gdt:
    lgdt (%rdi)
    pushq $0x08
    lea 1f(%rip), %rax
    push %rax
    lretq
1:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    xor %eax, %eax
    mov %ax, %fs
    mov %ax, %gs
    mov $0x28, %ax
    ltr %ax
    ret

.global arch_user_test_enter
.type arch_user_test_enter, @function
arch_user_test_enter:
    # Preserve the C caller's callee-saved state for the test completion path.
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15
    mov %rsp, user_saved_rsp(%rip)
    mov $0x1b, %ax
    mov %ax, %ds
    mov %ax, %es
    pushq $0x1b
    push %rsi
    pushq $0x202
    pushq $0x23
    push %rdi
    mov %rdx, %rdi
    xor %eax, %eax
    xor %ebx, %ebx
    xor %ecx, %ecx
    xor %edx, %edx
    xor %ebp, %ebp
    xor %esi, %esi
    xor %r8d, %r8d
    xor %r9d, %r9d
    xor %r10d, %r10d
    xor %r11d, %r11d
    xor %r12d, %r12d
    xor %r13d, %r13d
    xor %r14d, %r14d
    xor %r15d, %r15d
    iretq

.global arch_user_test_return
.type arch_user_test_return, @function
arch_user_test_return:
    cli
    cld
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %rdi, %rax
    mov user_saved_rsp(%rip), %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
    ret

.section .bss
.balign 8
user_saved_rsp: .skip 8
.section .note.GNU-stack,"",@progbits
