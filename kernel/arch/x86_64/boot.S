.section .multiboot,"a"
.balign 8
header:
    .long 0xe85250d6
    .long 0
    .long header_end - header
    .long -(0xe85250d6 + (header_end - header))
    # Require the memory-map boot information tag (type 6).
    .short 1, 0
    .long 12
    .long 6
    .long 0
    .short 0, 0
    .long 8
header_end:

.section .text
.code32
.global _start
.type _start, @function
_start:
    cli
    cld
    mov $stack_top, %esp
    mov %eax, boot_magic
    mov %ebx, boot_info
    # Check CPUID availability before checking long-mode support.
    pushfl
    pop %eax
    mov %eax, %ecx
    xor $0x200000, %eax
    push %eax
    popfl
    pushfl
    pop %eax
    push %ecx
    popfl
    xor %ecx, %eax
    test $0x200000, %eax
    jz unsupported
    mov $0x80000000, %eax
    cpuid
    cmp $0x80000001, %eax
    jb unsupported
    mov $0x80000001, %eax
    cpuid
    test $0x20000000, %edx
    jz unsupported

    # Identity-map the first GiB using 512 two-MiB pages.
    mov $pdpt, %eax
    or $3, %eax
    mov %eax, pml4
    mov $page_directory, %eax
    or $3, %eax
    mov %eax, pdpt
    xor %ecx, %ecx
1:
    mov %ecx, %eax
    shl $21, %eax
    or $0x83, %eax
    mov %eax, page_directory(,%ecx,8)
    inc %ecx
    cmp $512, %ecx
    jne 1b

    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4
    mov $pml4, %eax
    mov %eax, %cr3
    mov $0xc0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr
    lgdt gdt_pointer
    mov %cr0, %eax
    or $0x80000000, %eax
    mov %eax, %cr0
    ljmp $8, $long_mode
unsupported:
    movw $0x4f45, 0xb8000
2:
    hlt
    jmp 2b

.code64
long_mode:
    mov $16, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    xor %eax, %eax
    mov %ax, %fs
    mov %ax, %gs
    mov $stack_top, %rsp
    xor %ebp, %ebp
    mov boot_magic(%rip), %edi
    mov boot_info(%rip), %esi
    call kernel_main
3:
    cli
    hlt
    jmp 3b

.section .rodata
.balign 8
gdt:
    .quad 0
    .quad 0x00af9a000000ffff
    .quad 0x00cf92000000ffff
gdt_end:
gdt_pointer:
    .word gdt_end - gdt - 1
    .long gdt

.section .bss
.balign 4
boot_magic: .skip 4
boot_info: .skip 4
.balign 4096
pml4: .skip 4096
pdpt: .skip 4096
page_directory: .skip 4096
.balign 16
stack_bottom: .skip 16384
stack_top:
.section .note.GNU-stack,"",@progbits
