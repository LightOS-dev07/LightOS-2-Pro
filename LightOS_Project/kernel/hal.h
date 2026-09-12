#ifndef HAL_H
#define HAL_H
/*
 * kernel/hal.h — Hardware Abstraction Layer
 * 8/16/32-bit port I/O + BGA
 */
#include <stdint.h>

static inline void     hal_outb(uint16_t p,uint8_t  v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline void     hal_outw(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static inline void     hal_outl(uint16_t p,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t  hal_inb (uint16_t p){uint8_t  v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline uint16_t hal_inw (uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
static inline uint32_t hal_inl (uint16_t p){uint32_t v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void     hal_hlt (void)      {__asm__ volatile("hlt");}
static inline void     hal_cli (void)      {__asm__ volatile("cli");}

static inline void bga_write(uint16_t idx,uint16_t val){
    hal_outw(0x01CE,idx); hal_outw(0x01CF,val);
}
static inline void bga_set_mode(uint16_t w,uint16_t h,uint16_t bpp){
    bga_write(4,0); bga_write(1,w); bga_write(2,h);
    bga_write(3,bpp); bga_write(4,0x41);
}
#endif
