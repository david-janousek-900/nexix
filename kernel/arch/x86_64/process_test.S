.section .rodata.process_test,"a",@progbits
.code64
.global process_blob_start, process_blob_end
process_blob_start:
    mov $0x1234, %ebx
    mov $0x2345, %r12d
    mov $0x3456, %r13d
    mov $0x40004000, %r14d
    pushq $0x6789
    mov $0x100, %eax
    int $0x80
    test %rax, %rax
    js failed
    jz child
    mov %rax, %r15
parent_spin:
    mov $0x104, %eax
    int $0x80
    cmp $10, %rax
    jb parent_spin
    mov $0x101, %eax
    int $0x80
    cmp %r15, %rax
    jne failed
    cmp $0x2a00, %rdx
    jne failed
    cmpq $0x1111, (%r14)
    jne failed
    cmpq $0x6789, (%rsp)
    jne failed
    call check_regs
    xor %edi, %edi
    jmp done
child:
    cmpq $0x1111, (%r14)
    jne failed
    cmpq $0x6789, (%rsp)
    jne failed
    movq $0x2222, (%r14)
    movq $0x789a, (%rsp)
child_spin:
    mov $0x104, %eax
    int $0x80
    cmp $10, %rax
    jb child_spin
    call check_regs
    mov $0x2a00, %edi
done:
    mov $0x102, %eax
    int $0x80
    ud2
check_regs:
    cmp $0x1234, %rbx
    jne failed
    cmp $0x2345, %r12
    jne failed
    cmp $0x3456, %r13
    jne failed
    ret
failed:
    mov $0x1ff, %eax
    int $0x80
    ud2
process_blob_end:
.section .note.GNU-stack,"",@progbits
