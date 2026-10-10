ENTRY(_start)
PHDRS {
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}
SECTIONS {
    . = 1M;
    kernel_start = .;
    .multiboot : { KEEP(*(.multiboot)) } :text
    kernel_text_start = ALIGN(4K);
    .text ALIGN(4K) : { *(.text*) } :text
    kernel_text_end = ALIGN(4K);
    kernel_rodata_start = ALIGN(4K);
    .rodata ALIGN(4K) : { *(.rodata*) } :rodata
    kernel_rodata_end = ALIGN(4K);
    .data ALIGN(4K) : { *(.data*) } :data
    kernel_bss_start = ALIGN(4K);
    .bss ALIGN(4K) : { *(COMMON) *(.bss*) } :data
    kernel_end = ALIGN(4K);
    /DISCARD/ : { *(.eh_frame*) *(.comment) *(.note*) }
}
