.text
.globl core_fault
.globl core_fault_site
.type core_fault,@function
core_fault:
    movabs $0x123456789abcdef0,%r12
    movdqu core_fp_pattern(%rip),%xmm0
    movabs $0x700000000000,%rax
core_fault_site:
    movq (%rax),%rax
    ud2
.size core_fault,.-core_fault
.section .rodata
.globl core_fp_pattern
core_fp_pattern:
    .quad 0x1020304050607080,0x8877665544332211
.section .note.GNU-stack,"",@progbits
