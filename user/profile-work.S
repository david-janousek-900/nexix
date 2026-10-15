.section .text
.global profile_work
.type profile_work,@function
profile_work:
1:  pause
    sub $1,%rdi
    jnz 1b
    ret
.size profile_work,.-profile_work
.global profile_gate
.global profile_gate_return
.type profile_gate,@function
profile_gate:
    mov %rdi,%rax
    mov %rsi,%rdi
    mov %rdx,%rsi
    int $0x81
profile_gate_return:
    ret
.size profile_gate,.-profile_gate
.section .note.GNU-stack,"",@progbits
