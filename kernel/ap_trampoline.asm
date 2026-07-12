; kernel/ap_trampoline.asm
; AP startup code copied to physical 0x8000 (SIPI vector 0x08).
; APs start in 16-bit real mode. They:
;   1. Enable A20 (already on by BSP)
;   2. Load the BSP's GDT (descriptor placed by BSP at AP_GDT_OFF)
;   3. Enter 32-bit protected mode
;   4. Enable PAE, load PML4 (address at AP_PML4_OFF), enable long mode
;   5. Jump to 64-bit ap_entry (BSP fills in address at AP_ENTRY_OFF)
;
; BSP writes these values at (0x8000 + offset) before SIPI:
;   AP_GDT_OFF   equ 0xF0  : 6-byte GDT pseudo-descriptor (limit:base)
;   AP_PML4_OFF  equ 0xF8  : 4-byte PML4 physical address
;   AP_ENTRY_OFF equ 0xFC  : ... not used; we call ap_main via 64-bit trampoline
;   AP_STACK_OFF equ 0xE0  : 8-byte stack pointer
;   AP_FLAG_OFF  equ 0xE8  : 1-byte ready flag

bits 16
org 0x8000

AP_GDT_OFF   equ 0xF0
AP_PML4_OFF  equ 0xF8
AP_STACK_OFF equ 0xE0
AP_FLAG_OFF  equ 0xE8

ap_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Load GDT pseudo-descriptor placed by BSP
    lgdt [AP_GDT_OFF]

    ; Enter protected mode
    mov eax, cr0
    or  al, 1
    mov cr0, eax

    ; Far jump to flush pipeline and load CS with 32-bit code segment
    jmp dword 0x08:(.pm32 - ap_start + 0x8000)

bits 32
.pm32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Enable PAE
    mov eax, cr4
    or  eax, (1 << 5)
    mov cr4, eax

    ; Load PML4 (address stored at AP_PML4_OFF)
    mov eax, [AP_PML4_OFF]
    mov cr3, eax

    ; Enable Long Mode (EFER.LME)
    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1 << 8)
    wrmsr

    ; Enable paging (activates long mode)
    mov eax, cr0
    or  eax, (1 << 31)
    mov cr0, eax

    ; Far jump to 64-bit code
    jmp dword 0x08:(.lm64 - ap_start + 0x8000)

bits 64
.lm64:
    mov ax, 0x10
    mov ds, ax
    mov ss, ax

    ; Load AP stack
    mov rsp, [AP_STACK_OFF]
    xor rbp, rbp

    ; Signal BSP that this AP is running
    mov byte [AP_FLAG_OFF], 1

    ; Spin loop — BSP will assign work via ap_main pointer
    ; For now: enable interrupts and halt
    sti
.spin:
    hlt
    jmp .spin

    ; Pad to 256 bytes so BSP data area starts at offset 0xE0+
    times 0xE0 - ($ - $$) db 0

; Data area (BSP fills these before SIPI)
ap_stack_ptr:   dq 0            ; offset 0xE0
ap_ready_flag:  db 0            ; offset 0xE8
                times 7 db 0    ; align to 0xF0

ap_gdt_desc:    dw 0            ; offset 0xF0 : GDT limit
                dq 0            ; offset 0xF2 : GDT base (6 bytes total)
                dw 0            ; padding
ap_pml4:        dd 0            ; offset 0xF8
