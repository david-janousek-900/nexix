.section .text
.code64
.altmacro
.macro STUB number
.type isr_\number, @function
isr_\number:
    # These architectural exceptions push their own error code.
    .if (\number != 8) && (\number != 10) && (\number != 11) && (\number != 12) && (\number != 13) && (\number != 14) && (\number != 17) && (\number != 21) && (\number != 29) && (\number != 30)
        pushq $0
    .endif
    pushq $\number
    jmp interrupt_common
.endm
.set vector, 0
.rept 256
    STUB %vector
    .set vector, vector + 1
.endr
interrupt_common:
    cld
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
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %rsp, %rdi
    mov %rsp, %rbx
    and $-16, %rsp
    call interrupt_dispatch
    mov %rbx, %rsp
.global arch_interrupt_return
arch_interrupt_return:
    # Restore data selectors for the privilege level we are returning to.
    mov $0x10, %ax
    testb $3, 144(%rsp)
    jz 1f
    mov $0x1b, %ax
1:
    mov %ax, %ds
    mov %ax, %es
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
    add $16, %rsp
    iretq
.section .rodata
.balign 8
.global isr_table
isr_table:
.macro TABLE_ENTRY number
    .quad isr_\number
.endm
.set vector, 0
.rept 256
    TABLE_ENTRY %vector
    .set vector, vector + 1
.endr
.section .note.GNU-stack,"",@progbits
