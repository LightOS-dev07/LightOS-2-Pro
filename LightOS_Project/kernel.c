/* kernel.c — LightOS Kernel + Hardware Init */
/* Copyright Xaef BTL LightOS (c) 2026 */
#include <stdint.h>

/* ─────────────────────────────────────────────────────────────────
   Multiboot1 Info Yapısı (tam, 116 byte)
   ───────────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t flags;             /* 0  */
    uint32_t mem_lower;         /* 4  */
    uint32_t mem_upper;         /* 8  */
    uint32_t boot_device;       /* 12 */
    uint32_t cmdline;           /* 16 */
    uint32_t mods_count;        /* 20 */
    uint32_t mods_addr;         /* 24 */
    uint8_t  syms[16];          /* 28 */
    uint32_t mmap_length;       /* 44 */
    uint32_t mmap_addr;         /* 48 */
    uint32_t drives_length;     /* 52 */
    uint32_t drives_addr;       /* 56 */
    uint32_t config_table;      /* 60 */
    uint32_t boot_loader_name;  /* 64 */
    uint32_t apm_table;         /* 68 */
    uint32_t vbe_control_info;  /* 72 */
    uint32_t vbe_mode_info;     /* 76 */
    uint16_t vbe_mode;          /* 80 */
    uint16_t vbe_interface_seg; /* 82 */
    uint16_t vbe_interface_off; /* 84 */
    uint16_t vbe_interface_len; /* 86 */
    uint64_t framebuffer_addr;  /* 88 */
    uint32_t framebuffer_pitch; /* 96 */
    uint32_t framebuffer_width; /* 100*/
    uint32_t framebuffer_height;/* 104*/
    uint8_t  framebuffer_bpp;   /* 108*/
    uint8_t  framebuffer_type;  /* 109*/
    uint8_t  color_info[6];     /* 110*/
} __attribute__((packed)) mb_info_t;

/* ─────────────────────────────────────────────────────────────────
   Global Framebuffer Değişkenleri
   ───────────────────────────────────────────────────────────────── */
uint32_t* vram           = (uint32_t*)0xFD000000; /* QEMU BGA varsayılan */
uint32_t  screen_w       = 800;
uint32_t  screen_h       = 600;
uint32_t  screen_pitch   = 800 * 4;   /* byte cinsinden satır genişliği */

/* ─────────────────────────────────────────────────────────────────
   MSR Yardımcıları (MTRR için)
   ───────────────────────────────────────────────────────────────── */
static void wrmsr(uint32_t reg, uint64_t val) {
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile("wrmsr" :: "c"(reg), "a"(lo), "d"(hi));
}
static uint64_t rdmsr(uint32_t reg) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(reg));
    return ((uint64_t)hi << 32) | lo;
}

/* ─────────────────────────────────────────────────────────────────
   CPUID
   ───────────────────────────────────────────────────────────────── */
static int cpuid_check_mtrr(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid"
        : "=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx)
        : "0"(1));
    return (edx >> 12) & 1; /* bit 12 = MTRR */
}

/* ─────────────────────────────────────────────────────────────────
   MTRR — Framebuffer için Write-Combining
   Write-Combining: CPU, VRAM yazılarını paketler → ~4× hızlı
   ───────────────────────────────────────────────────────────────── */
static void SetupVRAM_WC(uint64_t fb_addr, uint32_t fb_size) {
    if (!cpuid_check_mtrr()) return;

    /* MTRR'leri etkinleştir (IA32_MTRRCAP = 0xFE, IA32_MTRR_DEF_TYPE = 0x2FF) */
    uint64_t def = rdmsr(0x2FF);
    if (!(def & (1ULL << 11))) return; /* MTRR disable */

    /* Boyutu 2'nin kuvvetine yuvarla */
    uint32_t sz = 1;
    while (sz < fb_size) sz <<= 1;

    /*
     * MTRR_PHYSBASE0 = 0x200, MTRR_PHYSMASK0 = 0x201
     * type = 1 (Write-Combining)
     * PHYSADDR_VALID bit = bit 11 of mask
     */
    /* Önce var olan MTRR'yi kontrol et, boş birini bul */
    uint64_t cap = rdmsr(0xFE);
    int n_var = (int)(cap & 0xFF);
    if (n_var > 8) n_var = 8;

    for (int i = 0; i < n_var; i++) {
        uint64_t base = rdmsr(0x200 + 2*i);
        uint64_t mask = rdmsr(0x201 + 2*i);
        if (!(mask & (1ULL<<11))) {
            /* Bu slot boş, kullan */
            wrmsr(0x200 + 2*i, fb_addr | 0x01); /* WC = type 1 */
            wrmsr(0x201 + 2*i, (~(uint64_t)(sz-1) & 0x0000FFFFFFFFF000ULL) | (1ULL<<11));
            return;
        }
    }
}

/* ─────────────────────────────────────────────────────────────────
   PIC — IRQ'ları maskele (bare metal'da sahte interrupt olmasın)
   ───────────────────────────────────────────────────────────────── */
static void outb_k(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));
}
static void PIC_disable(void) {
    /* PIC'leri yeniden programla (remap), sonra tümünü maskele */
    outb_k(0x20, 0x11); outb_k(0xA0, 0x11);
    outb_k(0x21, 0x20); outb_k(0xA1, 0x28); /* IRQ0-7 → INT 32-39 */
    outb_k(0x21, 0x04); outb_k(0xA1, 0x02);
    outb_k(0x21, 0x01); outb_k(0xA1, 0x01);
    outb_k(0x21, 0xFF); outb_k(0xA1, 0xFF); /* hepsini maskele */
}

/* ─────────────────────────────────────────────────────────────────
   Kernel Main
   ───────────────────────────────────────────────────────────────── */
extern void start_shell(void);

void kernel_main(mb_info_t* mb) {
    PIC_disable();

    /* Framebuffer bilgisini multiboot'tan al */
    if (mb && (mb->flags & (1 << 12)) && mb->framebuffer_bpp == 32) {
        vram         = (uint32_t*)(uint32_t)mb->framebuffer_addr;
        screen_w     = mb->framebuffer_width;
        screen_h     = mb->framebuffer_height;
        screen_pitch = mb->framebuffer_pitch;

        /* Framebuffer boyutunu hesapla, MTRR ile WC ayarla */
        uint32_t fb_size = screen_pitch * screen_h;
        SetupVRAM_WC((uint64_t)(uint32_t)vram, fb_size);
    }

    /* Cache disable/enable doğru sırayla → sonra WBINVD */
    __asm__ volatile(
        "wbinvd\n"          /* cache flush */
        "mov %%cr0, %%eax\n"
        "and $0xBFFFFFFF, %%eax\n" /* CR0.CD temizle */
        "mov %%eax, %%cr0\n"
        ::: "eax"
    );

    start_shell();
}
