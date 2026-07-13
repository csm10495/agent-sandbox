/*
 * Link SableOS freestanding user programs as ordinary static Linux ET_EXEC
 * binaries loaded at the conventional non-PIE base of 4 MiB. The kernel's ELF
 * loader maps each PT_LOAD segment at its p_vaddr, so the layout here matches
 * what the loader expects.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x400000;
    .text : ALIGN(0x1000) { *(.text*) }
    .rodata : ALIGN(0x1000) { *(.rodata*) }
    .data : ALIGN(0x1000) { *(.data*) }
    .bss : ALIGN(0x1000) { *(.bss*) *(COMMON) }

    /DISCARD/ : { *(.eh_frame*) *(.note*) *(.comment) }
}
