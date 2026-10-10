# Position-independent bytes copied into an RX user page by the bootstrap test.
.section .rodata.user,"a"
.code64
.balign 16
.global user_blob_start, user_blob_end
.global user_blob_success, user_blob_kernel, user_blob_ro, user_blob_nx
.global user_blob_priv, user_blob_io, user_blob_int, user_blob_bad_stack
user_blob_start:
user_blob_success:
    mov $0x1111, %ebx
    mov $0x2222, %ecx
    mov $0x3333, %edx
    mov $0x4444, %ebp
    mov $0x5555, %esi
    mov $0x6666, %edi
    mov $0x8888, %r8d
    mov $0x9999, %r9d
    mov $0xaaaa, %r10d
    mov $0xbbbb, %r11d
    mov $0xcccc, %r12d
    mov $0xdddd, %r13d
    mov $0xeeee, %r14d
    mov $0xffff, %r15d
    pushq $0x12345678
    pop %rax
    cmp $0x12345678, %eax
    jne user_failed
    xor %eax, %eax
    std
    int $0x81
    cmp $0x1234, %eax
    jne user_failed
    pushfq
    pop %rax
    test $0x400, %eax
    jz user_failed
    cld
user_wait:
    mov $1, %eax
    int $0x81
    cmp $10, %eax
    jb user_wait
    mov $2, %eax
    int $0x81
user_failed:
    mov $255, %eax
    int $0x81
    ud2
user_blob_kernel:
    mov (%rdi), %rax
    jmp user_failed
user_blob_ro:
    movb $0, (%rdi)
    jmp user_failed
user_blob_nx:
    jmp *%rdi
user_blob_priv:
    cli
    jmp user_failed
user_blob_io:
    mov $0x3f8, %edx
    xor %eax, %eax
    outb %al, %dx
    jmp user_failed
user_blob_int:
    int $0x20
    jmp user_failed
user_blob_bad_stack:
    mov $3, %eax
    int $0x81
    jmp user_failed
user_blob_end:
.section .note.GNU-stack,"",@progbits
