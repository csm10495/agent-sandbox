; boot/stage2.asm - Stage 2 Bootloader
; Loaded to 0x7E00 by stage1. Runs in 16-bit real mode.
; Responsibility: enable A20, load kernel to 0x10000 via BIOS,
; enter 32-bit protected mode, copy kernel to 0x100000,
; set up 4-level paging, enter 64-bit long mode, jump to kernel.

bits 16
org 0x7E00

KERNEL_LOAD_SEG   equ 0x1000     ; Real-mode load target: 0x1000*16 = 0x10000
KERNEL_LOAD_OFF   equ 0x0000
KERNEL_PHYS       equ 0x00100000 ; Final kernel physical address
KERNEL_START_LBA  equ 16         ; Kernel starts at LBA 16 on disk
KERNEL_MAX_SECTS  equ 512        ; Max 512 sectors = 256 KB (enough)
STACK2            equ 0x7C00     ; Stage2 stack (just below stage2)
PML4_ADDR         equ 0x00001000 ; Page tables at 0x1000-0x4FFF
PDPT_ADDR         equ 0x00002000
PD_ADDR           equ 0x00003000
PD2_ADDR          equ 0x00004000
BOOT_MAGIC        equ 0xB007B007 ; Our custom disk-boot magic

start2:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, STACK2
    sti

    ; Save boot drive
    mov [boot_drv], dl

    call enable_a20
    call load_kernel

    ; Print OK
    mov si, msg_ok
    call print16

    call setup_gdt

    ; Disable interrupts and enter protected mode
    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    jmp 0x08:.pm32_entry

; ---- 32-bit Protected Mode ------------------------------------------------
bits 32
.pm32_entry:
    mov ax, 0x10          ; data segment (index 2 * 8 = 0x10)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, STACK2

    ; Copy kernel from 0x10000 to 0x100000
    mov esi, 0x00010000
    mov edi, KERNEL_PHYS
    mov ecx, KERNEL_MAX_SECTS * 512 / 4   ; dword count
    rep movsd

    ; Set up page tables for 64-bit long mode
    call setup_paging_32

    ; Enable PAE
    mov eax, cr4
    or  eax, (1 << 5)
    mov cr4, eax

    ; Load CR3 with PML4 address
    mov eax, PML4_ADDR
    mov cr3, eax

    ; Enable Long Mode (LME) in EFER MSR 0xC0000080
    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1 << 8)
    wrmsr

    ; Enable paging (activates long mode since LME is set)
    mov eax, cr0
    or  eax, (1 << 31)
    mov cr0, eax

    ; Reload GDT with 64-bit code descriptor, then far-jump
    lgdt [gdt64_desc]
    jmp 0x08:.lm64_entry

; ---- 64-bit Long Mode -----------------------------------------------------
bits 64
.lm64_entry:
    ; Reload data segments with 64-bit descriptor
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up a proper 64-bit stack (8KB below kernel)
    mov rsp, KERNEL_PHYS - 8

    ; Pass boot magic and info=0 to kernel
    mov eax, BOOT_MAGIC      ; EAX = magic (kernel checks this)
    xor rbx, rbx             ; RBX = 0 (no multiboot info)
    xor rbp, rbp

    ; Jump to kernel entry at 0x100000
    mov rax, KERNEL_PHYS
    jmp rax

; ============================================================
bits 16
; ---- A20 Line Enable (Fast A20 via port 0x92) ----------------------
enable_a20:
    in  al, 0x92
    test al, 2
    jnz .done
    or  al, 2
    and al, 0xFE
    out 0x92, al
.done:
    ret

; ---- Load kernel from disk to 0x10000 via BIOS INT 13h AH=42h ------
load_kernel:
    mov si, msg_kern
    call print16

    mov si, kern_dap
    mov ah, 0x42
    mov dl, [boot_drv]
    int 0x13
    jc  .disk_err
    ret
.disk_err:
    mov si, msg_derr
    call print16
    cli
    hlt

print16:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bx, 7
    int 0x10
    jmp print16
.done:
    ret

; ---- GDT Setup (for 32-bit PM entry, also 64-bit descriptors) -------
setup_gdt:
    ; Already defined in data; just return (lgdt called in-line)
    ret

; ---- Page table setup (called from 32-bit PM) -----------------------
bits 32
setup_paging_32:
    ; Zero PML4 + PDPT + PD + PD2 (4 pages = 16KB)
    mov edi, PML4_ADDR
    xor eax, eax
    mov ecx, 4 * 4096 / 4
    rep stosd

    ; PML4[0] -> PDPT  (present + writable)
    mov dword [PML4_ADDR],     PDPT_ADDR | 0x03
    mov dword [PML4_ADDR + 4], 0

    ; PDPT[0] -> PD  (0 - 1GB)
    mov dword [PDPT_ADDR],     PD_ADDR | 0x03
    mov dword [PDPT_ADDR + 4], 0

    ; PDPT[1] -> PD2 (1 - 2GB)
    mov dword [PDPT_ADDR + 8],  PD2_ADDR | 0x03
    mov dword [PDPT_ADDR + 12], 0

    ; PD[0..511] -> 2MB huge pages (identity map 0 - 1GB)
    mov edi, PD_ADDR
    mov eax, 0x83             ; present + writable + PS (2MB)
    xor ebx, ebx
    mov ecx, 512
.pd1_loop:
    mov [edi],     eax
    mov [edi + 4], ebx
    add eax, 0x00200000       ; +2MB
    adc ebx, 0
    add edi, 8
    loop .pd1_loop

    ; PD2[0..511] -> 2MB huge pages (identity map 1 - 2GB)
    mov edi, PD2_ADDR
    mov eax, 0x00200000 | 0x83
    xor ebx, ebx
    mov ecx, 512
.pd2_loop:
    mov [edi],     eax
    mov [edi + 4], ebx
    add eax, 0x00200000
    adc ebx, 0
    add edi, 8
    loop .pd2_loop

    ret

; ============================================================
bits 16
align 8
; ---- GDT (used for 32-bit protected mode entry and 64-bit) ---------
gdt_start:
    dq 0                         ; null descriptor
    ; 0x08 - 32-bit code (for PM entry)
    dw 0xFFFF, 0x0000, 0x9A00, 0x00CF
    ; 0x10 - 32/64-bit data
    dw 0xFFFF, 0x0000, 0x9200, 0x00CF
gdt_end:

gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; 64-bit GDT
align 8
gdt64_start:
    dq 0                         ; null
    ; 0x08 - 64-bit code: L=1, P=1, DPL=0, Type=0xA (exec/read)
    dq 0x00AF9A000000FFFF
    ; 0x10 - 64-bit data: P=1, DPL=0, Type=0x2 (read/write)
    dq 0x00AF92000000FFFF
gdt64_end:

gdt64_desc:
    dw gdt64_end - gdt64_start - 1
    dd gdt64_start
    dd 0                         ; high 32 bits (gdtr is 10 bytes)

; ---- Kernel Disk Address Packet -----------------------------------------
align 4
kern_dap:
    db 0x10
    db 0x00
    dw KERNEL_MAX_SECTS
    dw KERNEL_LOAD_OFF
    dw KERNEL_LOAD_SEG
    dd KERNEL_START_LBA
    dd 0

boot_drv:   db 0x80
msg_ok:     db "Stage2 OK", 0x0D, 0x0A, 0
msg_kern:   db "Loading kernel...", 0x0D, 0x0A, 0
msg_derr:   db "Disk error!", 0
