/* SandboxOS kernel linker script — higher-half, Limine-loaded ELF64.
 *
 * Limine's "Limine" protocol loads the kernel ELF anywhere the bootloader
 * chooses, but a kernel compiled for a higher-half virtual address works
 * correctly because Limine maps the kernel to its requested virtual addresses.
 * We place the kernel at 0xffffffff80000000 (the top 2 GiB of virtual
 * address space), per the standard higher-half convention.
 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
    text    PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ; /* Execute + Read */
    rodata  PT_LOAD    FLAGS((1 << 2)) ;            /* Read only */
    data    PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ; /* Write + Read */
    dynamic PT_DYNAMIC FLAGS((1 << 1) | (1 << 2)) ;
}

SECTIONS
{
    . = 0xffffffff80000000;

    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(4K);
    .rodata : {
        KEEP(*(.requests_start_marker))
        KEEP(*(.requests))
        KEEP(*(.requests_end_marker))
        *(.rodata .rodata.*)
    } :rodata

    .note.gnu.build-id : { *(.note.gnu.build-id) } :rodata
    .eh_frame_hdr      : { *(.eh_frame_hdr) }      :rodata
    .eh_frame          : { *(.eh_frame) }          :rodata

    . = ALIGN(4K);
    .data : {
        *(.data .data.*)
    } :data

    .dynamic : { *(.dynamic) } :data :dynamic

    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    /DISCARD/ : {
        *(.note .note.*)
        *(.comment)
    }
}
