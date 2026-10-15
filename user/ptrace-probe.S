.section .text
.p2align 3
.global trace_register_probe
.type trace_register_probe,@function
trace_register_probe:
    push %r12
    mov $0x12345678,%r12
    mov $0x76543210,%eax
    movq %rax,%xmm0
    mov $15,%edi
    call raise
    cmp $0x23456789,%r12
    jne 1f
    movq %xmm0,%rax
    cmp $0x3456789a,%rax
    jne 1f
    xor %eax,%eax
    pop %r12
    ret
1:  mov $97,%eax
    pop %r12
    ret
.size trace_register_probe,.-trace_register_probe
.global trace_redirect
.type trace_redirect,@function
trace_redirect:
    mov $73,%edi
    call _exit
.size trace_redirect,.-trace_redirect
.section .note.GNU-stack,"",@progbits
.section .text
.global trace_step_start,trace_step_mov,trace_step_syscall,trace_step_nop
.global trace_step_break,trace_step_afterbreak,trace_write_fault,trace_data_jump
trace_step_start:
    inc %r12
trace_step_mov:
    mov $20,%eax
trace_step_syscall:
    int $0x80
trace_step_nop:
    nop
trace_step_break:
    int3
trace_step_afterbreak:
    mov $78,%edi
    and $-16,%rsp
    call _exit
trace_write_fault:
    movb $0,(%rdi)
    ud2
trace_data_jump:
    jmp *%rdi
.global trace_step_fork,trace_step_fork_return
trace_step_fork:
    mov $2,%eax
    int $0x80
trace_step_fork_return:
    test %eax,%eax
    jz 2f
    mov %rax,%r12
    sub $16,%rsp
    mov %rsp,%rdi
    mov $7,%eax
    int $0x80
    cmp %r12,%rax
    jne 3f
    cmpl $(79<<8),(%rsp)
    jne 3f
    jmp trace_step_afterbreak
2:  mov $79,%edi
    jmp 4f
3:  mov $80,%edi
4:  and $-16,%rsp
    call _exit
