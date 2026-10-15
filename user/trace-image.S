.section .text
.p2align 3
.global trace_patch_target
.type trace_patch_target,@function
trace_patch_target:
    mov $41,%eax
    ret
    nop
    nop
.size trace_patch_target,.-trace_patch_target
.section .note.GNU-stack,"",@progbits
