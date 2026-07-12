; boot/stage1.asm - Stage 1 MBR Bootloader (512 bytes)
; Loads stage2 (sectors 1-15) to 0x7E00 and jumps there.
; Custom in-repo bootloader - no external dependencies.

bits 16
org 0x7C00

STAGE2_LOAD_SEG  equ 0x07E0   ; 0x07E0 * 16 = 0x7E00
STAGE2_LOAD_OFF  equ 0x0000
STAGE2_SECTORS   equ 15

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    ; Print loading message
    mov si, msg_load
    call print16

    ; Use BIOS INT 13h AH=42h (Extended LBA Read)
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc  .disk_err

    jmp STAGE2_LOAD_SEG:STAGE2_LOAD_OFF

.disk_err:
    mov si, msg_err
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

; Disk Address Packet
align 4
dap:
    db 0x10               ; size of DAP = 16 bytes
    db 0x00               ; reserved
    dw STAGE2_SECTORS     ; number of sectors to read
    dw STAGE2_LOAD_OFF    ; destination offset
    dw STAGE2_LOAD_SEG    ; destination segment
    dd 1                  ; LBA low dword  (sector 1 onward)
    dd 0                  ; LBA high dword

boot_drive: db 0x80

msg_load db "Loading...", 0x0D, 0x0A, 0
msg_err  db "Disk error!", 0

    times 446 - ($ - $$) db 0   ; pad to partition table offset
    times 64 db 0                ; empty partition table
    dw 0xAA55                    ; boot signature
