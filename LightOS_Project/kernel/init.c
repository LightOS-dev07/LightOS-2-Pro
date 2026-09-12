#include <stdint.h>
#include "../drivers/serial.h"

typedef struct {
    uint32_t flags, mem_lower, mem_upper;
    uint32_t boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint8_t  syms[16];
    uint32_t mmap_length, mmap_addr;
    uint32_t drives_length, drives_addr;
    uint32_t config_table, boot_loader_name, apm_table;
    uint32_t vbe_ctrl, vbe_info;
    uint16_t vbe_mode, vbe_seg, vbe_off, vbe_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch, framebuffer_width, framebuffer_height;
    uint8_t  framebuffer_bpp, framebuffer_type, color_info[6];
} __attribute__((packed)) mb_info_t;

/* Global framebuffer */
uint32_t* vram         = (uint32_t*)0xE0000000;
uint32_t  screen_w     = 800;
uint32_t  screen_h     = 600;
uint32_t  screen_pitch = 800*4;
uint32_t  mem_lower_kb = 0;
uint32_t  mem_upper_kb = 0;

static inline void outb(uint16_t p,uint8_t  v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline void outw(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static inline void outl(uint16_t p,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static inline uint32_t inl(uint16_t p){uint32_t v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}

static void pic_init(void){
    outb(0x20,0x11); outb(0xA0,0x11);
    outb(0x21,0x20); outb(0xA1,0x28);
    outb(0x21,0x04); outb(0xA1,0x02);
    outb(0x21,0x01); outb(0xA1,0x01);
    outb(0x21,0xFF); outb(0xA1,0xFF);
}

static void bga_set(uint16_t i,uint16_t v){ outw(0x01CE,i); outw(0x01CF,v); }

/* PCI config space (mechanism #1, port 0xCF8/0xCFC) */
static uint32_t pci_read32(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off){
    uint32_t addr = 0x80000000u | ((uint32_t)bus<<16) | ((uint32_t)dev<<11)
                  | ((uint32_t)fn<<8) | (off&0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

/* VGA/display controller'ı PCI bus 0 üzerinde tarar (class 0x03, subclass 0x00),
 * bulursa BAR0'ın gerçek fiziksel adresini döner (prefetchable/memory BAR'lar
 * için alt 4 bit maskelenir). Bulunamazsa 0 döner ve çağıran taraf sabit
 * bir varsayılana (0xE0000000) düşer.
 *
 * Bu tarama gereklidir çünkü QEMU'nun -vga std cihazı framebuffer'ını
 * sabit bir adrese değil, PCI BAR0'a (host'a göre değişebilen, bu ortamda
 * örn. 0xFD000000 gibi bir adrese) yerleştirir. Sabit 0xE0000000 varsayımı
 * yanlış adrese yazmaya, yani ekranın hep siyah kalmasına yol açıyordu. */
static uint32_t pci_find_vga_bar0(void){
    for(uint16_t dev=0; dev<32; dev++){
        uint32_t id = pci_read32(0,(uint8_t)dev,0,0x00);
        if(id==0xFFFFFFFFu) continue; /* cihaz yok */
        uint32_t classreg = pci_read32(0,(uint8_t)dev,0,0x08);
        uint8_t base_class = (uint8_t)(classreg>>24);
        uint8_t sub_class  = (uint8_t)(classreg>>16);
        if(base_class==0x03 && sub_class==0x00){ /* VGA compatible controller */
            uint32_t bar0 = pci_read32(0,(uint8_t)dev,0,0x10);
            if(bar0 & 0x1) continue; /* I/O space BAR, framebuffer değil */
            return bar0 & 0xFFFFFFF0u; /* alt 4 bit: type/prefetch bitleri */
        }
    }
    return 0;
}

static void wrmsr(uint32_t r,uint64_t v){
    __asm__ volatile("wrmsr"::"c"(r),"a"((uint32_t)v),"d"((uint32_t)(v>>32)));
}
static uint64_t rdmsr(uint32_t r){
    uint32_t lo,hi;
    __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(r));
    return((uint64_t)hi<<32)|lo;
}
static void setup_wc(uint32_t addr,uint32_t size){
    uint32_t a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"0"(1));
    if(!((d>>12)&1)) return;
    if(!(rdmsr(0x2FF)&(1ULL<<11))) return;
    uint32_t sz=1; while(sz<size) sz<<=1;
    int n=(int)(rdmsr(0xFE)&0xFF); if(n>8)n=8;
    for(int i=0;i<n;i++){
        if(!(rdmsr(0x201+2*i)&(1ULL<<11))){
            wrmsr(0x200+2*i,(uint64_t)addr|0x01);
            wrmsr(0x201+2*i,(~(uint64_t)(sz-1)&0x0000FFFFFFFFF000ULL)|(1ULL<<11));
            return;
        }
    }
}

extern void start_shell(void);

/* 64-bit entry: rdi = mb_info ptr (System V AMD64 ABI) */
void kernel_main(mb_info_t* mb){
    serial_init(COM1, BAUD_115200);
    serial_log("kernel_main: boot started");

    pic_init();

    if(mb&&(mb->flags&1)){
        mem_lower_kb = mb->mem_lower;
        mem_upper_kb = mb->mem_upper;
    }

    /* Framebuffer: GRUB → BGA fallback */
    if(mb&&(mb->flags&(1<<12))&&mb->framebuffer_bpp==32&&mb->framebuffer_addr){
        vram         = (uint32_t*)(uintptr_t)mb->framebuffer_addr;
        screen_w     = mb->framebuffer_width;
        screen_h     = mb->framebuffer_height;
        screen_pitch = mb->framebuffer_pitch;
        serial_log("kernel_main: framebuffer = GRUB");
    } else {
        bga_set(4,0); bga_set(1,1024); bga_set(2,768);
        bga_set(3,32); bga_set(4,0x41);
        uint32_t pci_bar = pci_find_vga_bar0();
        vram = (uint32_t*)(uintptr_t)(pci_bar ? pci_bar : 0xE0000000UL);
        screen_w     = 1024;
        screen_h     = 768;
        screen_pitch = 1024*4;
        serial_log(pci_bar ? "kernel_main: framebuffer = BGA fallback (PCI BAR0)"
                            : "kernel_main: framebuffer = BGA fallback (default 0xE0000000)");
    }
    serial_puts(COM1,"[LightOS] vram = 0x");
    serial_hex(COM1, (uint32_t)(uintptr_t)vram);
    serial_puts(COM1," resolution: ");
    serial_hex(COM1, screen_w);
    serial_puts(COM1," x ");
    serial_hex(COM1, screen_h);
    serial_puts(COM1," pitch ");
    serial_hex(COM1, screen_pitch);
    serial_puts(COM1,"\n");

    setup_wc((uint32_t)(uintptr_t)vram, screen_pitch*screen_h);
    /* Cache enable */
    uint64_t cr0;
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    cr0 &= ~((uint64_t)1<<30); /* CD=0 */
    cr0 &= ~((uint64_t)1<<29); /* NW=0 */
    __asm__ volatile("mov %0,%%cr0"::"r"(cr0));

    serial_log("kernel_main: entering start_shell");
    start_shell();

    /* start_shell() normalde hiç dönmez (GUI ana döngüsü sonsuzdur).
     * Buraya düşülmesi beklenmeyen bir durumdur. */
    serial_log("kernel_main: start_shell returned unexpectedly");
    __asm__ volatile("cli");
    for(;;) __asm__ volatile("hlt");
}
