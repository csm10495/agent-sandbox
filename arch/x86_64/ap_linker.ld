ENTRY(ap_start)
SECTIONS {
    . = 0x8000;
    .text : { *(.text*) *(.rodata*) }
    /DISCARD/ : { *(.note*) *(.eh_frame*) }
}
