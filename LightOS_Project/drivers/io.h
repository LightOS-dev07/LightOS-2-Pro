#ifndef IO_H
#define IO_H
/*
 * drivers/io.h — Hardware I/O Port Access
 * ==========================================
 * Tüm port okuma/yazma işlemleri buradan.
 * 8-bit, 16-bit, 32-bit destekli.
 * 32-bit safe (m32 veya native).
 */
#include <stdint.h>

/* ── Byte (8-bit) ── */
static inline void     outb(uint16_t p,uint8_t  v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t  inb (uint16_t p){uint8_t  v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}

/* ── Word (16-bit) ── */
static inline void     outw(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static inline uint16_t inw (uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}

/* ── DWord (32-bit) ── */
static inline void     outl(uint16_t p,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static inline uint32_t inl (uint16_t p){uint32_t v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}

/* ── IO wait (eski donanım için gecikme) ── */
static inline void io_wait(void){outb(0x80,0);}

/* ── Port okuma/yazma kısa macro'lar ── */
#define OUTB(p,v) outb((p),(v))
#define INB(p)    inb((p))
#define OUTW(p,v) outw((p),(v))
#define INW(p)    inw((p))

/* ── C++ extern compat ── */
#ifdef __cplusplus
extern "C" {
#endif
void    outb_c(uint16_t port, uint8_t val);
uint8_t inb_c (uint16_t port);
#ifdef __cplusplus
}
#endif

#endif
