; Stage 2 — Ultra minimal
[BITS 16]
[ORG 0x0500]

    cli
    xor  ax,ax
    mov  ds,ax
    mov  es,ax
    mov  ss,ax
    mov  sp,0x7BFF
    sti

    ; VGA text debug
    mov  ax,0xB800
    mov  es,ax
    mov  word [es:0],0x4F4C  ; L
    mov  word [es:2],0x4F49  ; I
    xor  ax,ax
    mov  es,ax

    ; A20 fast
    in   al,0x92
    or   al,0x02
    and  al,0xFE
    out  0x92,al

    mov  ax,0xB800
    mov  es,ax
    mov  word [es:4],0x0A41  ; A green
    xor  ax,ax
    mov  es,ax

    ; VESA bilgisi: kernel.c BGA'yı kendisi ayarlıyor
    ; Sadece ok=1 yaz, addr=0 bırak → kernel fallback kullanır
    mov  dword [0x0600], 0           ; addr = 0 → kernel BGA'yı kurar
    mov  dword [0x0604], 3200        ; pitch
    mov  word  [0x0608], 800
    mov  word  [0x060A], 600
    mov  byte  [0x060C], 32
    mov  byte  [0x060D], 0           ; ok=0 → GRUB path → kernel BGA kurar

    ; Kernel yükle
    call load_kernel

    mov  ax,0xB800
    mov  es,ax
    mov  word [es:6],0x0A4B  ; K green
    xor  ax,ax
    mov  es,ax

    ; GDT + PM
    lgdt [gdt_ptr]
    cli
    mov  eax,cr0
    or   eax,1
    mov  cr0,eax
    jmp  0x08:pm32

; ── Kernel yükle (LBA) ──
load_kernel:
    mov  word  [cur_seg],  0x1000
    mov  dword [cur_lba],  34
.lp:
    mov  bx, [cur_seg]
    mov  [dap+6], bx
    mov  word  [dap+4],  0
    mov  byte  [dap+0],  16
    mov  byte  [dap+1],  0
    mov  word  [dap+2],  127
    mov  eax,  [cur_lba]
    mov  [dap+8], eax
    mov  dword [dap+12], 0
    mov  ah,  0x42
    mov  dl,  0x80
    mov  si,  dap
    int  0x13
    jc   .done
    add  word  [cur_seg], 0x1000
    add  dword [cur_lba], 127
    cmp  word  [cur_seg], 0x9000
    jb   .lp
.done:
    ret

align 8
gdt_tbl:
    dq 0
    dw 0xFFFF,0x0000,0x9A00,0x00CF
    dw 0xFFFF,0x0000,0x9200,0x00CF
gdt_end:
gdt_ptr:
    dw gdt_end-gdt_tbl-1
    dd 0x0500+(gdt_tbl-$$)   ; fiziksel adres = ORG(0x500) + offset

cur_seg: dw 0
cur_lba: dd 0
dap:     times 16 db 0

[BITS 32]
pm32:
    mov  ax,0x10
    mov  ds,ax
    mov  es,ax
    mov  fs,ax
    mov  gs,ax
    mov  ss,ax
    mov  esp,0x9FC00

    ; Debug P
    mov  word [0xB8000+8],0x0A50  ; P

    ; SSE etkinleştir
    mov  eax,cr0
    and  eax,0xFFFFFFFB
    or   eax,0x00000002
    mov  cr0,eax
    mov  eax,cr4
    or   eax,0x00000600
    mov  cr4,eax
    fninit

    ; Debug S
    mov  word [0xB8000+10],0x0A53  ; S

    ; Kernel kopyala: 0x10000 → 0x100000 (512KB)
    mov  esi,0x00010000
    mov  edi,0x00100000
    mov  ecx,8*16384
    rep  movsd

    ; Debug C
    mov  word [0xB8000+12],0x0A43  ; C

    ; Stack kur ve kernel'e geç
    mov  esp,0x9FC00
    sub  esp,8
    mov  dword [esp],   0xDEADBEEF
    mov  dword [esp+4], 0x00000600
    jmp  0x00100000

times 16384-($-$$) db 0
