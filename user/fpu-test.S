#include <nexix/syscall.h>
.text
.global fpu_process_test
.type fpu_process_test,@function
fpu_process_test:
    push %rbx
    push %r12
    sub $24, %rsp
    mov $0x301, %eax
    int $0x81
    mov $0x11223344, %ebx
    movq %rbx, %xmm0
    movq %rbx, %xmm15
    fld1
    movw $0x77f, 8(%rsp)
    fldcw 8(%rsp)
    movl $0x3f80, 12(%rsp)
    ldmxcsr 12(%rsp)
    mov $-1, %r12
    mov $NX_SYS_FORK, %eax
    int $0x80
    test %rax, %rax
    js .Lfail
    mov %rax, %r12
    movq %xmm0, %rax
    cmp %rbx, %rax
    jne .Lfail
    movq %xmm15, %rax
    cmp %rbx, %rax
    jne .Lfail
    fstl (%rsp)
    movabs $0x3ff0000000000000, %rax
    cmp %rax, (%rsp)
    jne .Lfail
    fnstcw 10(%rsp)
    cmpw $0x77f, 10(%rsp)
    jne .Lfail
    stmxcsr 16(%rsp)
    cmpl $0x3f80, 16(%rsp)
    jne .Lfail
    test %r12, %r12
    jne .Lparent
    inc %rbx
    movq %rbx, %xmm0
    movq %rbx, %xmm15
    movw $0xb7f, 8(%rsp)
    fldcw 8(%rsp)
    movl $0x5f80, 12(%rsp)
    ldmxcsr 12(%rsp)
.Lparent:
    mov $4, %esi
.Lloop:
    mov $NX_SYS_YIELD, %eax
    int $0x80
    # Long enough to exercise timer preemption as well as voluntary switches.
    mov $50000000, %ecx
.Lspin:
    dec %ecx
    jne .Lspin
    movq %xmm0, %rax
    cmp %rbx, %rax
    jne .Lfail
    movq %xmm15, %rax
    cmp %rbx, %rax
    jne .Lfail
    fnstcw 10(%rsp)
    movzwl 8(%rsp), %eax
    cmp %ax, 10(%rsp)
    jne .Lfail
    stmxcsr 16(%rsp)
    mov 12(%rsp), %eax
    cmp %eax, 16(%rsp)
    jne .Lfail
    fstl (%rsp)
    movabs $0x3ff0000000000000, %rax
    cmp %rax, (%rsp)
    jne .Lfail
    dec %esi
    jne .Lloop
    test %r12, %r12
    je .Lchildok
    mov %rsp, %rdi
    mov $NX_SYS_WAIT, %eax
    int $0x80
    cmp %r12, %rax
    jne .Lfail
    cmpl $0, (%rsp)
    jne .Lfail
    mov $0x302, %eax
    int $0x81
    fninit
    movl $0x1f80, 12(%rsp)
    ldmxcsr 12(%rsp)
    mov $1, %eax
    jmp .Lreturn
.Lchildok:
    xor %edi, %edi
    mov $NX_SYS_EXIT, %eax
    int $0x80
    ud2
.Lfail:
    test %r12, %r12
    je .Lchildfail
    fninit
    movl $0x1f80, 12(%rsp)
    ldmxcsr 12(%rsp)
    xor %eax, %eax
.Lreturn:
    add $24, %rsp
    pop %r12
    pop %rbx
    ret
.Lchildfail:
    mov $1, %edi
    mov $NX_SYS_EXIT, %eax
    int $0x80
    ud2
.size fpu_process_test,.-fpu_process_test
.section .note.GNU-stack,"",@progbits
