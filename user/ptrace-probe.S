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
