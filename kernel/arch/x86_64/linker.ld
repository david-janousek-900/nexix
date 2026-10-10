ENTRY(_start)
PHDRS {
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}
SECTIONS {
    . = 1M;
    .multiboot : { KEEP(*(.multiboot)) } :text
    .text ALIGN(4K) : { *(.text*) } :text
    .rodata ALIGN(4K) : { *(.rodata*) } :rodata
    .data ALIGN(4K) : { *(.data*) } :data
    .bss ALIGN(4K) : { *(COMMON) *(.bss*) } :data
    kernel_end = ALIGN(4K);
    /DISCARD/ : { *(.eh_frame*) *(.comment) *(.note*) }
}
