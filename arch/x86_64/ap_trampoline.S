.section .text
.code16
.global ap_start
ap_start:
    cli
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    lgdt ap_gdt_ptr

    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4
    mov 0x8f00, %eax
    mov %eax, %cr3
    mov $0xc0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr
    mov %cr0, %eax
    or $((1 << 31) | 1), %eax
    mov %eax, %cr0
    ljmpl $0x08, $ap_long_mode

.code64
ap_long_mode:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0x8f08, %rax
    mov (%rax), %rsp
    xor %rbp, %rbp
    mov $0x8f10, %rax
    call *(%rax)
1:
    hlt
    jmp 1b

.align 8
ap_gdt:
    .quad 0
    .quad 0x00af9a000000ffff
    .quad 0x00af92000000ffff
ap_gdt_ptr:
    .word . - ap_gdt - 1
    .long ap_gdt

.section .note.GNU-stack,"",@progbits
