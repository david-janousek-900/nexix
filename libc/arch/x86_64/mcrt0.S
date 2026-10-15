/* Native counterpart of V7 profiling startup. Hardware FP is always present;
 * the fmcrt0 build shares this path rather than installing a PDP-11 emulator. */
.section .text
.global _start
.type _start,@function
_start:
    xor %ebp,%ebp
    cld
    mov %rsp,%r12
    mov (%rsp),%rdi
    lea 16(%rsp,%rdi,8),%rdx
    mov %rdx,environ(%rip)
    and $-16,%rsp
    lea __nx_profile_low(%rip),%rdi
    lea __nx_text_end(%rip),%rsi
    call __nx_profile_start
    test %eax,%eax
    js 1f
    mov (%r12),%rdi
    lea 8(%r12),%rsi
    lea 16(%r12,%rdi,8),%rdx
    call main
    mov %eax,%edi
    call exit
1:  mov $4,%eax
    mov $2,%edi
    lea no_space(%rip),%rsi
    mov $(no_space_end-no_space),%edx
    int $0x80
    mov $127,%edi
    call _exit
.size _start,.-_start
__nx_profile_low:
.section .rodata
no_space: .ascii "No space for monitor buffer\n"
no_space_end:
.section .note.GNU-stack,"",@progbits
