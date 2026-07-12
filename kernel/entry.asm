; kernel/entry.asm
; Kernel entry point: Multiboot2 header + 32-bit -> 64-bit transition.
; GRUB loads us in 32-bit protected mode (EAX=MB2_MAGIC, EBX=info_ptr).
; Stage2 disk boot arrives here already in 64-bit mode (EAX=BOOT_MAGIC).
; Both paths call kernel_main(uint32_t magic, uint64_t mbi_addr).

MULTIBOOT2_MAGIC    equ 0xE85250D6
DISK_BOOT_MAGIC     equ 0xB007B007

PML4_ADDR   equ 0x00001000
PDPT_ADDR   equ 0x00002000
PD_ADDR     equ 0x00003000
PD2_ADDR    equ 0x00004000
PD3_ADDR    equ 0x00005000   ; PDPT entry 3 → maps 3-4 GB (LAPIC at 0xFEE00000)

STACK_SIZE  equ 0x4000       ; 16 KB boot stack in BSS

; ============================================================
; Multiboot2 Header (must be in first 32KB of kernel image)
; ============================================================
section .multiboot2
align 8
mb2_header_start:
    dd MULTIBOOT2_MAGIC
    dd 0                                        ; architecture: i386/x86
    dd (mb2_header_end - mb2_header_start)      ; header length
    dd -(MULTIBOOT2_MAGIC + 0 + (mb2_header_end - mb2_header_start)) ; checksum

    ; End tag
    dw 0, 0
    dd 8
mb2_header_end:

; ============================================================
; 32-bit entry point (from GRUB multiboot2)
; ============================================================
section .boot
bits 32
global kernel_entry
kernel_entry:
    ; Disable interrupts, set up stack
    cli
    mov esp, boot_stack_top

    ; Save multiboot magic and info pointer
    mov [mb2_magic_save], eax
    mov [mb2_info_save],  ebx

    ; Test if we were already in 64-bit (disk boot path via stage2)
    ; Stage2 already set up paging and jumped here in long mode;
    ; the ljmp lands in .lm_trampoline directly.
    ; But for safety, check EFER.LMA to detect long mode:
    mov ecx, 0xC0000080
    rdmsr
    test eax, (1 << 10)     ; LMA bit
    jnz .already_long       ; stage2 disk boot: already in long mode

    ; ---- GRUB path: set up 64-bit paging from scratch ----
    call .setup_paging

    ; Enable PAE
    mov eax, cr4
    or  eax, (1 << 5)
    mov cr4, eax

    ; Load PML4
    mov eax, PML4_ADDR
    mov cr3, eax

    ; Enable Long Mode in EFER
    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1 << 8)
    wrmsr

    ; Enable paging (activates LM because LME=1)
    mov eax, cr0
    or  eax, (1 << 31)
    mov cr0, eax

    ; Load 64-bit GDT and far-jump into long mode
    lgdt [gdt64_ptr]
    jmp 0x08:.lm_entry

.already_long:
    ; Disk-boot: we arrived here from stage2 already in 64-bit mode
    ; but via a 64-bit jmp that lands at kernel_entry (32-bit label).
    ; The stage2 gdt is still active; reload our own gdt64 now.
    lgdt [gdt64_ptr]
    jmp 0x08:.lm_entry

; ---- paging setup (32-bit helper) ----------------------------------
.setup_paging:
    ; Zero 5 pages: PML4, PDPT, PD, PD2, PD3 (at 0x1000-0x5FFF)
    mov edi, PML4_ADDR
    xor eax, eax
    mov ecx, 5 * 4096 / 4
    rep stosd

    mov dword [PML4_ADDR],     PDPT_ADDR | 0x03
    mov dword [PML4_ADDR + 4], 0

    ; PDPT[0] → 0-1 GB, PDPT[1] → 1-2 GB, PDPT[3] → 3-4 GB (LAPIC)
    mov dword [PDPT_ADDR],      PD_ADDR  | 0x03
    mov dword [PDPT_ADDR + 4],  0
    mov dword [PDPT_ADDR + 8],  PD2_ADDR | 0x03
    mov dword [PDPT_ADDR + 12], 0
    mov dword [PDPT_ADDR + 24], PD3_ADDR | 0x03   ; entry 3 (3*8=24)
    mov dword [PDPT_ADDR + 28], 0

    ; PD[0..511] -> 2MB huge pages (0 - 1 GB)
    mov edi, PD_ADDR
    mov eax, 0x83
    xor ebx, ebx
    mov ecx, 512
.pd1:
    mov [edi],     eax
    mov [edi + 4], ebx
    add eax, 0x200000
    adc ebx, 0
    add edi, 8
    loop .pd1

    ; PD2[0..511] -> 2MB huge pages (1 - 2 GB)
    mov edi, PD2_ADDR
    mov eax, 0x200000 | 0x83
    xor ebx, ebx
    mov ecx, 512
.pd2:
    mov [edi],     eax
    mov [edi + 4], ebx
    add eax, 0x200000
    adc ebx, 0
    add edi, 8
    loop .pd2

    ; PD3[503] → 0xFEE00000 (LAPIC, 2MB, cache-disabled, write-through)
    ; 0xFEE00000 in PDPT entry 3 (covers 3GB..4GB):
    ;   PD index = (0xFEE00000 >> 21) & 0x1FF = 0x1F7 = 503
    ;   offset within 2MB region = 0 (aligned)
    ; flags: present(0)|writable(1)|cache-disabled(4)|write-through(3)|huge(7)
    ; = 0x9B
    mov dword [PD3_ADDR + 503*8],     0xFEE00000 | 0x9B
    mov dword [PD3_ADDR + 503*8 + 4], 0

    ret

; ============================================================
; 64-bit entry
; ============================================================
bits 64
extern bss_start
extern bss_end
.lm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Save multiboot magic and info in callee-saved registers BEFORE clearing BSS.
    ; mb2_magic_save / mb2_info_save are in .bss, written by the 32-bit entry.
    ; Use [rel] for correct RIP-relative encoding in 64-bit mode.
    mov  r12d, dword [rel mb2_magic_save]
    mov  r13,  qword [rel mb2_info_save]

    ; Zero BSS (includes the boot stack, so this must happen BEFORE mov rsp).
    ; Uses rep stosb which clobbers rdi, rax, rcx.
    lea  rdi, [rel bss_start]
    xor  eax, eax
    lea  rcx, [rel bss_end]
    sub  rcx, rdi
    rep  stosb

    ; Now set up the boot stack (lives in BSS, freshly zeroed).
    lea  rsp, [rel boot_stack_top]
    xor  rbp, rbp

    ; Call kernel_main(uint32_t magic, uint64_t mbi_addr)
    mov  edi, r12d
    mov  rsi, r13

    extern kernel_main
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

; ============================================================
; GDT64
; ============================================================
section .rodata
align 8
gdt64:
    dq 0                     ; null
    dq 0x00AF9A000000FFFF    ; 0x08: 64-bit code (L=1, P=1, DPL=0)
    dq 0x00AF92000000FFFF    ; 0x10: 64-bit data (P=1, DPL=0)
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dq gdt64

; ============================================================
; Boot BSS
; ============================================================
section .bss
align 16
mb2_magic_save: resd 1
mb2_info_save:  resq 1
global boot_stack_top
boot_stack:
    resb STACK_SIZE
boot_stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits
