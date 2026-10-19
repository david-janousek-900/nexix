#include <nexix/syscall.h>
#include <nexix/errno.h>
.section .rodata.syscall_test,"a",@progbits
.code64
.set DATA, 0x40004000
.set READONLY, 0x40006000
.set HIGH, 0x8000000000
.macro CALL number
    mov $\number, %eax
    int $0x80
.endm
.macro EXPECT value
    cmp $\value, %rax
    jne failed
.endm
.macro REG reg, value
    movabs $\value, %rax
    cmp %rax, %\reg
    jne failed
.endm
.global syscall_blob_start, syscall_blob_end
syscall_blob_start:
    CALL 0
    EXPECT 0
    CALL 45
    EXPECT -NX_EINVAL
    movabs $0x100000002, %rax  # Must not truncate into fork.
    int $0x80
    EXPECT -NX_EINVAL
    movabs $-1, %rax
    int $0x80
    EXPECT -NX_EINVAL
    mov $0x999, %eax          # Inactive test commands cannot execute hooks.
    int $0x81
    EXPECT -NX_ENOSYS

    # All six arguments, all GPRs except RAX, and CF/DF/IF survive the ABI.
    mov $0x1111, %ebx
    mov $0x2222, %ebp
    mov $0x3333, %r12d
    mov $0x4444, %r13d
    mov $0x5555, %r14d
    mov $0x6666, %r15d
    movabs $0x1111222233334444, %rdi
    movabs $0x2222333344445555, %rsi
    movabs $0x3333444455556666, %rdx
    movabs $0x4444555566667777, %r10
    movabs $0x5555666677778888, %r8
    movabs $0x6666777788889999, %r9
    mov $0x7777, %ecx
    mov $0x8888, %r11d
    std
    stc
    CALL NX_SYS_GETPID
    pushfq
    pop %rax
    and $0x601, %eax
    EXPECT 0x601
    REG rbx, 0x1111
    REG rbp, 0x2222
    REG r12, 0x3333
    REG r13, 0x4444
    REG r14, 0x5555
    REG r15, 0x6666
    REG rdi, 0x1111222233334444
    REG rsi, 0x2222333344445555
    REG rdx, 0x3333444455556666
    REG r10, 0x4444555566667777
    REG r8, 0x5555666677778888
    REG r9, 0x6666777788889999
    REG rcx, 0x7777
    REG r11, 0x8888
    mov $0x200, %eax
    int $0x81
    cld

    CALL NX_SYS_GETPID
    test %rax, %rax
    jle failed
    mov %rax, %r12
    CALL NX_SYS_GETPPID
    EXPECT 0
    CALL NX_SYS_GETUID
    EXPECT 123
    CALL NX_SYS_GETEUID
    EXPECT 321
    CALL NX_SYS_GETGID
    EXPECT 654
    CALL NX_SYS_GETEGID
    EXPECT 456
    mov $0xffff, %edi
    CALL NX_SYS_UMASK
    EXPECT 022
    mov $022, %edi
    CALL NX_SYS_UMASK
    EXPECT 0777
    CALL NX_SYS_YIELD
    EXPECT 0

    # Input spans two pages; copy output must preserve the full high pointer.
    mov $DATA+4092, %edi
    movl $0x64636261, (%rdi)
    movw $0x0065, 4(%rdi)
    movabs $HIGH+16, %rsi
    mov $0x201, %eax
    int $0x81
    movabs $0xabcdef0123456789, %rax
    cmp %rax, (%rsi)
    jne failed

    xor %edi, %edi
    CALL NX_SYS_WAIT
    EXPECT -NX_ECHILD
    CALL NX_SYS_TIMES
    EXPECT -NX_EFAULT
    mov $0x100000, %edi
    CALL NX_SYS_TIMES
    EXPECT -NX_EFAULT
    mov $READONLY, %edi
    CALL NX_SYS_TIMES
    EXPECT -NX_EFAULT
    mov $0x40007000, %edi
    CALL NX_SYS_TIMES
    EXPECT -NX_EFAULT
    movabs $0xffff800000000000, %rdi
    CALL NX_SYS_TIMES
    EXPECT -NX_EFAULT
    movabs $0x800000000000, %rdi
    CALL NX_SYS_TIMES
    EXPECT -NX_EFAULT
    movabs $-16, %rdi
    CALL NX_SYS_TIMES
    EXPECT -NX_EFAULT
    movabs $0x7ffffffffff0, %rdi
    CALL NX_SYS_TIMES
    EXPECT -NX_EFAULT

    # Failed cross-page copyout must not touch the writable prefix.
    mov $READONLY-16, %edi
    movabs $0x1122334455667788, %rax
    mov %rax, (%rdi)
    mov %rax, 8(%rdi)
    CALL NX_SYS_TIMES
    EXPECT -NX_EFAULT
    movabs $0x1122334455667788, %rax
    cmp %rax, (%rdi)
    jne failed
    cmp %rax, 8(%rdi)
    jne failed
    # Valid unaligned cross-page copyout and subsequent error reset.
    mov $DATA+4096-8, %edi
    CALL NX_SYS_TIMES
    test %rax, %rax
    js failed
    CALL NX_SYS_GETPID
    cmp %r12, %rax
    jne failed

    mov $DATA, %r14d
    movq $0x1111, (%r14)
    pushq $0x6789
    CALL NX_SYS_FORK
    test %rax, %rax
    js failed
    jz child
    mov %rax, %r15
    mov $READONLY, %edi
    CALL NX_SYS_WAIT
    EXPECT -NX_EFAULT       # Invalid output must not consume the child.
    movabs $HIGH+129, %rdi
    CALL NX_SYS_WAIT
    cmp %r15, %rax
    jne failed
    cmpl $0x2a00, (%rdi)
    jne failed
    cmpq $0x1111, (%r14)
    jne failed
    cmpq $0x6789, (%rsp)
    jne failed
    xor %edi, %edi
    CALL NX_SYS_WAIT
    EXPECT -NX_ECHILD
    mov $DATA+128, %edi
    CALL NX_SYS_TIMES
    test %rax, %rax
    js failed
    cmpq $5, 16(%rdi)
    jb failed
    mov $0x100, %edi         # exit status is the low eight bits, thus zero.
    CALL NX_SYS_EXIT
    ud2
child:
    CALL NX_SYS_GETPPID
    cmp %r12, %rax
    jne failed
    CALL NX_SYS_GETPID
    cmp %r12, %rax
    je failed
    movq $0x2222, (%r14)
    movq $0x789a, (%rsp)
child_wait:
    mov $0x202, %eax
    int $0x81
    test %rax, %rax
    jnz child_spin
    CALL NX_SYS_YIELD
    jmp child_wait
child_spin:
    mov $DATA+128, %edi
    CALL NX_SYS_TIMES
    test %rax, %rax
    js failed
    cmpq $5, (%rdi)
    jb child_spin
    mov $42, %edi
    CALL NX_SYS_EXIT
    ud2
failed:
    cld
    mov $99, %edi
    CALL NX_SYS_EXIT
    ud2
syscall_blob_end:
.section .note.GNU-stack,"",@progbits
