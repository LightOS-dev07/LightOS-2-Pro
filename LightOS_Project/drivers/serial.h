#ifndef SERIAL_H
#define SERIAL_H
/*
 * drivers/serial.h — Serial Port (UART 16550A)
 * =============================================
 * COM1: 0x3F8, COM2: 0x2F8, COM3: 0x3E8, COM4: 0x2E8
 * 32-bit güvenli (port IO 16-bit, her yerde çalışır)
 * 
 * Kullanım:
 *   serial_init(COM1, BAUD_115200);
 *   serial_puts(COM1, "LightOS Debug\n");
 *   char c = serial_getc(COM1);
 */
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

/* Port base adresleri */
#define COM1 0x3F8
#define COM2 0x2F8
#define COM3 0x3E8
#define COM4 0x2E8

/* Baud divisors (clock=1.8432MHz) */
#define BAUD_115200  1
#define BAUD_57600   2
#define BAUD_38400   3
#define BAUD_19200   6
#define BAUD_9600   12

/* UART register offsets */
#define UART_DATA    0   /* RX/TX */
#define UART_IER     1   /* Interrupt Enable */
#define UART_FCR     2   /* FIFO Control */
#define UART_LCR     3   /* Line Control */
#define UART_MCR     4   /* Modem Control */
#define UART_LSR     5   /* Line Status */
#define UART_MSR     6   /* Modem Status */
#define UART_DLL     0   /* Divisor Low  (DLAB=1) */
#define UART_DLH     1   /* Divisor High (DLAB=1) */

/* LSR bits */
#define LSR_TX_EMPTY 0x20
#define LSR_RX_READY 0x01

static void uart_outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static uint8_t uart_inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}

/* Seri port başlat */
static bool serial_init(uint16_t port, uint16_t baud_div) {
    /* Disable interrupts */
    uart_outb(port + UART_IER, 0x00);
    /* DLAB=1: baud rate ayarla */
    uart_outb(port + UART_LCR, 0x80);
    uart_outb(port + UART_DLL, (uint8_t)(baud_div & 0xFF));
    uart_outb(port + UART_DLH, (uint8_t)(baud_div >> 8));
    /* 8N1: 8 bit, no parity, 1 stop bit */
    uart_outb(port + UART_LCR, 0x03);
    /* FIFO: enable, clear, 14-byte threshold */
    uart_outb(port + UART_FCR, 0xC7);
    /* Modem control: DTR+RTS */
    uart_outb(port + UART_MCR, 0x03);
    /* Loopback test */
    uart_outb(port + UART_MCR, 0x1E);
    uart_outb(port + UART_DATA, 0xAE);
    if(uart_inb(port + UART_DATA) != 0xAE) return false;
    /* Normal mode */
    uart_outb(port + UART_MCR, 0x0F);
    return true;
}

/* TX hazır mı? */
static inline bool serial_tx_ready(uint16_t port) {
    return (uart_inb(port + UART_LSR) & LSR_TX_EMPTY) != 0;
}

/* RX veri var mı? */
static inline bool serial_rx_ready(uint16_t port) {
    return (uart_inb(port + UART_LSR) & LSR_RX_READY) != 0;
}

/* Tek karakter gönder */
static void serial_putc(uint16_t port, char c) {
    while(!serial_tx_ready(port));
    uart_outb(port + UART_DATA, (uint8_t)c);
}

/* String gönder */
static void serial_puts(uint16_t port, const char* s) {
    while(*s) {
        if(*s == '\n') serial_putc(port, '\r');
        serial_putc(port, *s++);
    }
}

/* Karakter al (bloklamaz — 0 döner yoksa) */
static char serial_getc_nb(uint16_t port) {
    if(!serial_rx_ready(port)) return 0;
    return (char)uart_inb(port + UART_DATA);
}

/* Log fonksiyonu — COM1'e debug yaz */
static void serial_log(const char* msg) {
    serial_puts(COM1, "[LightOS] ");
    serial_puts(COM1, msg);
    serial_puts(COM1, "\n");
}

/* Int'i hex string olarak yaz */
static void serial_hex(uint16_t port, uint32_t v) {
    const char* hx = "0123456789ABCDEF";
    serial_puts(port, "0x");
    for(int i=28;i>=0;i-=4)
        serial_putc(port, hx[(v>>i)&0xF]);
}

#endif
