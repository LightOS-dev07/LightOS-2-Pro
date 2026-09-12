; ================================================================
; LightOS boot.asm — Multiboot1 → Long Mode (64-bit)
; ================================================================
; Kritik düzeltmeler:
;   1. Page table 0–4GB map (0xE0000000 dahil)
;   2. mcmodel=small → 1MB'de çalışır
;   3. Stack .data'da (BSS clear öncesi güvenli)
;   4. BSS kendi başlangıcından itibaren temizlenir
; ================================================================
bits 32

; ── Multiboot ─────────────────────────────────────────────────
section .multiboot
    MAGIC    equ 0x1BADB002
    FLAGS    equ (1<<1)|(1<<2)
    CHECKSUM equ -(MAGIC+FLAGS)
    dd MAGIC, FLAGS, CHECKSUM
    dd 0,0,0,0,0     ; load fields unused
    dd 0              ; linear fb
    dd 0,0,0          ; any res/depth

; ── Page Tables (4GB identity map, 2MB huge pages) ────────────
section .data
align 4096
pml4:   times 512 dq 0
align 4096
pdpt:   times 512 dq 0
align 4096
pd0:    times 512 dq 0    ; 0   – 1GB
align 4096
pd1:    times 512 dq 0    ; 1   – 2GB
align 4096
pd2:    times 512 dq 0    ; 2   – 3GB
align 4096
pd3:    times 512 dq 0    ; 3   – 4GB

; ── 64-bit GDT ────────────────────────────────────────────────
align 16
gdt64:
    dq 0                            ; null
.code: equ $ - gdt64
    dq 0x00AF9A000000FFFF           ; 64-bit code (L=1)
.data: equ $ - gdt64
    dq 0x00CF92000000FFFF           ; data
gdt64_ptr:
    dw $ - gdt64 - 1
    dq gdt64                        ; 64-bit base

; ── Stack (in .data → valid before BSS clear) ─────────────────
align 16
stack_bottom: times 65536 db 0
stack_top:

; ── 32-bit GDT (PM geçiş sonrası) ────────────────────────────
gdt32:
    dq 0
    dw 0xFFFF,0x0000,0x9A00,0x00CF   ; code 0x08
    dw 0xFFFF,0x0000,0x9200,0x00CF   ; data 0x10
gdt32_end:
gdt32_ptr:
    dw gdt32_end - gdt32 - 1
    dd gdt32

; ── BSS bounds ────────────────────────────────────────────────
section .bss
align 16
global __bss_start, __bss_end
__bss_start:
resb 0
__bss_end:
resb 0

; ── Entry ─────────────────────────────────────────────────────
section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top          ; stack .data'da, güvenli

    ; GDT32 + segment reload
    lgdt [gdt32_ptr]
    jmp 0x08:.r32
.r32:
    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov fs,ax
    mov gs,ax
    mov ss,ax

    ; EBX sakla (multiboot info ptr)
    mov edi, ebx

    ; ── Page table kur ──────────────────────────────────────
    ; PML4[0] → pdpt
    mov eax, pdpt
    or  eax, 0x03
    mov [pml4], eax

    ; PDPT[0..3] → pd0..pd3
    mov eax, pd0
    or  eax, 0x03
    mov [pdpt+0],  eax
    mov eax, pd1
    or  eax, 0x03
    mov [pdpt+8],  eax
    mov eax, pd2
    or  eax, 0x03
    mov [pdpt+16], eax
    mov eax, pd3
    or  eax, 0x03
    mov [pdpt+24], eax

    ; pd0: 0x000000 – 0x3FFFFFFF (512 × 2MB)
    mov ecx, 0
.pd0:
    mov eax, 0x200000
    mul ecx
    or  eax, 0x83          ; present+writable+huge
    mov [pd0 + ecx*8], eax
    inc ecx
    cmp ecx, 512
    jne .pd0

    ; pd1: 0x40000000 – 0x7FFFFFFF
    mov ecx, 0
.pd1:
    mov eax, 0x200000
    mul ecx
    add eax, 0x40000000
    or  eax, 0x83
    mov [pd1 + ecx*8], eax
    inc ecx
    cmp ecx, 512
    jne .pd1

    ; pd2: 0x80000000 – 0xBFFFFFFF
    mov ecx, 0
.pd2:
    mov eax, 0x200000
    mul ecx
    add eax, 0x80000000
    or  eax, 0x83
    mov [pd2 + ecx*8], eax
    inc ecx
    cmp ecx, 512
    jne .pd2

    ; pd3: 0xC0000000 – 0xFFFFFFFF  ← 0xE0000000 (VRAM) burada!
    mov ecx, 0
.pd3:
    mov eax, 0x200000
    mul ecx
    add eax, 0xC0000000
    or  eax, 0x83
    mov [pd3 + ecx*8], eax
    inc ecx
    cmp ecx, 512
    jne .pd3

    ; ── Long Mode aktif ────────────────────────────────────
    ; CR3 = PML4
    mov eax, pml4
    mov cr3, eax

    ; CR4.PAE + SSE
    mov eax, cr4
    or  eax, (1<<5)|(1<<9)|(1<<10)
    mov cr4, eax

    ; EFER.LME + NXE
    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1<<8)|(1<<11)
    wrmsr

    ; CR0: PG + PE + WP
    mov eax, cr0
    or  eax, (1<<31)|(1<<16)|(1<<0)
    mov cr0, eax

    ; 64-bit GDT ve far jump
    lgdt [gdt64_ptr]
    jmp 0x08:long_entry

; ── 64-bit giriş ──────────────────────────────────────────────
bits 64
long_entry:
    mov ax, 0x10
    mov ds,ax
    mov es,ax
    mov fs,ax
    mov gs,ax
    mov ss,ax
    mov rsp, stack_top          ; 64-bit stack

    ; BSS temizle
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    jle .bss_done
    shr rcx, 3                  ; qword sayısı
    xor rax, rax
    rep stosq
.bss_done:

    ; FPU/SSE reset
    fninit
    
    ; kernel_main(mb_info*) — System V ABI: arg1 = rdi
    ; edi'de sakladığımız multiboot ptr'yi rdi'ye geçir
    mov  edi, edi               ; zero-extend to rdi (64-bit clears high 32 bits)
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
