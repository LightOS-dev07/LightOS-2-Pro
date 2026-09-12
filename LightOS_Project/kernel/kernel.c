#include <stdint.h>
typedef struct {
    uint32_t flags,mem_lower,mem_upper,boot_device,cmdline;
    uint32_t mods_count,mods_addr;
    uint8_t  syms[16];
    uint32_t mmap_length,mmap_addr;
    uint32_t drives_length,drives_addr;
    uint32_t config_table,boot_loader_name,apm_table;
    uint32_t vbe_ctrl,vbe_info;
    uint16_t vbe_mode,vbe_seg,vbe_off,vbe_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch,framebuffer_width,framebuffer_height;
    uint8_t  framebuffer_bpp,framebuffer_type,color_info[6];
} __attribute__((packed)) mb_info_t;

uint32_t* vram=(uint32_t*)0xE0000000;
uint32_t  screen_w=800,screen_h=600,screen_pitch=800*4;
uint32_t  mem_lower_kb=0,mem_upper_kb=0;

static void outb_k(uint16_t p,uint8_t  v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static void outw_k(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static void pic_init(void){
    outb_k(0x20,0x11);outb_k(0xA0,0x11);
    outb_k(0x21,0x20);outb_k(0xA1,0x28);
    outb_k(0x21,0x04);outb_k(0xA1,0x02);
    outb_k(0x21,0x01);outb_k(0xA1,0x01);
    outb_k(0x21,0xFF);outb_k(0xA1,0xFF);
}
static void bga_set(uint16_t i,uint16_t v){outw_k(0x01CE,i);outw_k(0x01CF,v);}
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
    if(!((d>>12)&1))return;
    if(!(rdmsr(0x2FF)&(1ULL<<11)))return;
    uint32_t sz=1;while(sz<size)sz<<=1;
    int n=(int)(rdmsr(0xFE)&0xFF);if(n>8)n=8;
    for(int i=0;i<n;i++){
        if(!(rdmsr(0x201+2*i)&(1ULL<<11))){
            wrmsr(0x200+2*i,(uint64_t)addr|0x01);
            wrmsr(0x201+2*i,(~(uint64_t)(sz-1)&0x0000FFFFFFFFF000ULL)|(1ULL<<11));
            return;
        }
    }
}
extern void start_shell(void);
void kernel_main(mb_info_t* mb){
    pic_init();
    if(mb&&(mb->flags&1)){mem_lower_kb=mb->mem_lower;mem_upper_kb=mb->mem_upper;}
    if(mb&&(mb->flags&(1<<12))&&mb->framebuffer_bpp==32&&mb->framebuffer_addr){
        vram=(uint32_t*)(uint32_t)mb->framebuffer_addr;
        screen_w=mb->framebuffer_width;screen_h=mb->framebuffer_height;
        screen_pitch=mb->framebuffer_pitch;
    } else {
        bga_set(4,0);bga_set(1,800);bga_set(2,600);bga_set(3,32);bga_set(4,0x41);
        vram=(uint32_t*)0xE0000000;screen_pitch=800*4;
    }
    setup_wc((uint32_t)vram,screen_pitch*screen_h);
    __asm__ volatile("wbinvd\nmov %%cr0,%%eax\nand $0xBFFFFFFF,%%eax\nmov %%eax,%%cr0\n":::"eax");
    start_shell();
}
