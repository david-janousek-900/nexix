.text
.global abi_inspect
.type abi_inspect,@function
abi_inspect:
    cmp $8,%al
    jne .Lbad
    mov %rsp,%rax
    and $15,%eax
    cmp $8,%eax
    jne .Lbad
    cmp $1,%rdi
    jne .Lbad
    cmp $2,%rsi
    jne .Lbad
    cmp $3,%rdx
    jne .Lbad
    cmp $4,%rcx
    jne .Lbad
    cmp $5,%r8
    jne .Lbad
    cmp $6,%r9
    jne .Lbad
    cmpq $7,8(%rsp)
    jne .Lbad
    cmpq $8,16(%rsp)
    jne .Lbad
    .macro fp reg, expected
    cvttsd2si \reg,%rax
    cmp $\expected,%rax
    jne .Lbad
    .endm
    fp %xmm0,1
    fp %xmm1,2
    fp %xmm2,3
    fp %xmm3,4
    fp %xmm4,5
    fp %xmm5,6
    fp %xmm6,7
    fp %xmm7,8
    movsd 24(%rsp),%xmm0
    fp %xmm0,9
    pushfq
    pop %rax
    test $0x400,%eax
    jne .Lbad
    xor %eax,%eax
    ret
.Lbad:
    mov $1,%eax
    ret
.size abi_inspect,.-abi_inspect
.section .note.GNU-stack,"",@progbits
