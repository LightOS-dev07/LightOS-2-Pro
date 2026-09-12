#include "io.h"
extern "C" {
    void    outb_c(uint16_t port,uint8_t  val){ outb(port,val); }
    uint8_t inb_c (uint16_t port)             { return inb(port); }
}
