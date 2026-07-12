/* boot/linker.ld - Kernel linker script
 * Kernel is linked at physical 1MB (0x100000).
 * No higher-half mapping; identity-mapped by stage2 page tables.
 */

ENTRY(kernel_entry)

SECTIONS {
    . = 0x100000;
    kernel_phys_start = .;

    .boot ALIGN(16) : {
        KEEP(*(.multiboot2))
        KEEP(*(.boot))
    }

    .text ALIGN(4096) : {
        *(.text .text.*)
    }

    .rodata ALIGN(4096) : {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4096) : {
        *(.data .data.*)
    }

    .bss ALIGN(4096) : {
        bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        bss_end = .;
    }

    kernel_phys_end = .;

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
