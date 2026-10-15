ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); stack PT_GNU_STACK FLAGS(6); }
SECTIONS {
    . = 0x40000000;
    .text : { *(.text*) *(.rodata*) } :text
    PROVIDE(__nx_text_end = .);
    PROVIDE(etext = .);
    PROVIDE(_etext = .);
    . = ALIGN(4096);
    .data : { *(.data*) } :data
    .bss : { *(.bss*) } :data
    /DISCARD/ : { *(.note*) *(.eh_frame*) *(.comment*) }
}
