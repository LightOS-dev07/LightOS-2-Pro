[BITS 32]
[SECTION .bl_entry]
global bl_entry_start
extern kernel_main
extern __bss_start
extern __bss_end

bl_entry_start:
    ; Stage2'den geliyoruz — stack: [esp]=0xDEAD [esp+4]=0x600
    ; Önce BSS'yi temizle (linker sembollerini kullan)
    mov  edi, __bss_start
    mov  ecx, __bss_end
    sub  ecx, edi
    jle  .skip_bss
    shr  ecx, 2
    xor  eax, eax
    rep  stosd
.skip_bss:
    ; kernel_main(0x600) — stack hazır
    jmp  kernel_main
