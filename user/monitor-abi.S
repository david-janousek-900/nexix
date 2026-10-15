.section .text
.global monitor_abi
.type monitor_abi,@function
monitor_abi:
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15
    sub $40,%rsp
    fnstcw (%rsp)
    fldcw control(%rip)
    fld1
    movdqu pattern(%rip),%xmm0
    movdqu pattern(%rip),%xmm15
    mov $0x101,%eax
    mov $0x102,%ebx
    mov $0x103,%ecx
    mov $0x104,%edx
    mov $0x105,%ebp
    mov $0x106,%esi
    mov $0x107,%edi
    mov $0x108,%r8d
    mov $0x109,%r9d
    mov $0x110,%r10d
    mov $0x111,%r11d
    mov $0x112,%r12d
    mov $0x113,%r13d
    mov $0x114,%r14d
    mov $0x115,%r15d
    stc
    pushfq
    popq 8(%rsp)
    call mcount
    pushfq
    popq 16(%rsp)
    cmp $0x101,%rax
    jne 1f
    cmp $0x102,%rbx
    jne 1f
    cmp $0x103,%rcx
    jne 1f
    cmp $0x104,%rdx
    jne 1f
    cmp $0x105,%rbp
    jne 1f
    cmp $0x106,%rsi
    jne 1f
    cmp $0x107,%rdi
    jne 1f
    cmp $0x108,%r8
    jne 1f
    cmp $0x109,%r9
    jne 1f
    cmp $0x110,%r10
    jne 1f
    cmp $0x111,%r11
    jne 1f
    cmp $0x112,%r12
    jne 1f
    cmp $0x113,%r13
    jne 1f
    cmp $0x114,%r14
    jne 1f
    cmp $0x115,%r15
    jne 1f
    mov 8(%rsp),%rax
    xor 16(%rsp),%rax
    test $0x8d5,%eax
    jnz 1f
    movdqu pattern(%rip),%xmm1
    pcmpeqb %xmm1,%xmm0
    pmovmskb %xmm0,%eax
    cmp $65535,%eax
    jne 1f
    pcmpeqb %xmm1,%xmm15
    pmovmskb %xmm15,%eax
    cmp $65535,%eax
    jne 1f
    fnstcw 2(%rsp)
    cmpw $0x077f,2(%rsp)
    jne 1f
    fstpl 24(%rsp)
    movabs $0x3ff0000000000000,%rax
    cmp 24(%rsp),%rax
    jne 1f
    xor %eax,%eax
    jmp 2f
1:  mov $1,%eax
2:  fninit
    fldcw (%rsp)
    add $40,%rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
    ret
.size monitor_abi,.-monitor_abi
.section .rodata
control: .short 0x077f
pattern: .quad 0x0123456789abcdef,0xfedcba9876543210
.section .note.GNU-stack,"",@progbits
.section .text
.global monitor_site_a,monitor_site_b,monitor_site_c
monitor_site_a:
    sub $8,%rsp
    call mcount
    add $8,%rsp
    ret
monitor_site_b:
    sub $8,%rsp
    call mcount
    add $8,%rsp
    ret
monitor_site_c:
    sub $8,%rsp
    call mcount
    add $8,%rsp
    ret
