; ================================================================
; LightOS Stage 1 — MBR (512 bytes)
; ================================================================
; Disk'in ilk sektörüne yazılır.
; Stage 2'yi (sektör 2-17, 16 sektör = 8KB) yükler ve çalıştırır.
; Stage 2 adresi: 0x0500
; ================================================================
[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7C00
    sti

    ; Boot disk numarasını kaydet
    mov  [boot_drive], dl

    ; Stage 2 yükle (sektör 2'den başlayarak, 16 sektör, 0x0500'e)
    mov  ah, 0x02        ; BIOS read sectors
    mov  al, 32          ; 32 sektör = 16KB (stage2 = 16KB)
    mov  ch, 0           ; cylinder 0
    mov  cl, 2           ; sector 2 (1-indexed)
    mov  dh, 0           ; head 0
    mov  dl, [boot_drive]
    mov  bx, 0x0500      ; ES:BX = 0000:0500
    int  0x13
    jc   disk_error

    ; Stage 2'ye geç
    jmp  0x0000:0x0500

disk_error:
    mov  si, err_msg
.loop:
    lodsb
    test al, al
    jz   .halt
    mov  ah, 0x0E
    int  0x10
    jmp  .loop
.halt:
    hlt
    jmp .halt

boot_drive: db 0
err_msg:    db "Disk error!", 0

; Boot imzası
times 446-($-$$) db 0
; Partition tablosu (boş)
times 64 db 0
dw 0xAA55
