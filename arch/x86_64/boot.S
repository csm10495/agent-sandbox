.section .multiboot
.align 8
mb2_header:
    .long 0xe85250d6
    .long 0
    .long mb2_header_end - mb2_header
    .long -(0xe85250d6 + (mb2_header_end - mb2_header))
    .short 0
    .short 0
    .long 8
mb2_header_end:

.section .bss
.align 4096
pml4:
    .skip 4096
pdp:
    .skip 4096
pd:
    .skip 16384
.align 16
boot_stack_bottom:
    .skip 32768
boot_stack_top:

.section .rodata
.align 8
gdt64:
    .quad 0
    .quad 0x00af9a000000ffff
    .quad 0x00af92000000ffff
gdt64_ptr:
    .word . - gdt64 - 1
    .long gdt64

.section .text
.code32
.global _start
.type _start, @function
_start:
    cli
    mov %eax, %edi
    mov %ebx, %esi
    mov $boot_stack_top, %esp

    cmpl $0x36d76289, %edi
    jne boot_hang

    mov $pdp, %eax
    or $3, %eax
    mov %eax, pml4
    xor %ecx, %ecx
0:
    mov %ecx, %eax
    shl $12, %eax
    add $pd, %eax
    or $3, %eax
    mov %eax, pdp(,%ecx,8)
    inc %ecx
    cmp $4, %ecx
    jne 0b

    xor %ecx, %ecx
1:
    mov %ecx, %eax
    shl $21, %eax
    or $0x83, %eax
    mov %eax, pd(,%ecx,8)
    inc %ecx
    cmp $2048, %ecx
    jne 1b

    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4
    mov $pml4, %eax
    mov %eax, %cr3
    mov $0xc0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr
    mov %cr0, %eax
    or $(1 << 31), %eax
    mov %eax, %cr0
    lgdt gdt64_ptr
    ljmp $0x08, $long_mode

boot_hang:
    hlt
    jmp boot_hang

.code64
long_mode:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $boot_stack_top, %rsp
    xor %rbp, %rbp
    mov %esi, %edi
    call kernel_main
2:
    cli
    hlt
    jmp 2b

.section .note.GNU-stack,"",@progbits
