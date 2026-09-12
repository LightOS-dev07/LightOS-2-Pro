#ifndef RTL8139_H
#define RTL8139_H
/*
 * kernel/net/rtl8139.h — RTL8139 NIC Driver
 * ===========================================
 * QEMU: -device rtl8139,netdev=n0 -netdev user,id=n0
 * PCI ID: 0x10EC:0x8139
 */
#include "net.h"
#include "../hal.h"

/* RTL8139 register offsets */
#define RTL_IDR0     0x00   /* MAC address */
#define RTL_RBSTART  0x30   /* RX buffer start */
#define RTL_CMD      0x37   /* Command */
#define RTL_CAPR     0x38   /* Current address of packet read */
#define RTL_IMR      0x3C   /* Interrupt mask */
#define RTL_ISR      0x3E   /* Interrupt status */
#define RTL_TCR      0x40   /* TX config */
#define RTL_RCR      0x44   /* RX config */
#define RTL_9346CR   0x50   /* 93C46 command */
#define RTL_CONFIG1  0x52
#define RTL_TSAD0    0x20   /* TX status addr 0 */
#define RTL_TSD0     0x10   /* TX status 0 */

/* TX descriptor base addresses */
static const uint16_t RTL_TSAD[4]={0x20,0x24,0x28,0x2C};
static const uint16_t RTL_TSD[4] ={0x10,0x14,0x18,0x1C};

/* PCI */
static uint32_t pci_read32(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t reg){
    hal_outl(0xCF8,0x80000000u|((uint32_t)bus<<16)|((uint32_t)dev<<11)|((uint32_t)fn<<8)|(reg&0xFC));
    return hal_inl(0xCFC);
}
static void pci_write32(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t reg,uint32_t v){
    hal_outl(0xCF8,0x80000000u|((uint32_t)bus<<16)|((uint32_t)dev<<11)|((uint32_t)fn<<8)|(reg&0xFC));
    hal_outl(0xCFC,v);
}

struct RTL8139 {
    uint16_t io_base;
    uint8_t  tx_idx;
    bool     ready;
    uint8_t* tx_buf; /* 0x03000000 */
    uint8_t* rx_buf; /* 0x03006000 */

    bool init(){
        /* PCI tara */
        for(uint8_t bus=0;bus<8;bus++){
            for(uint8_t dev=0;dev<32;dev++){
                uint32_t id=pci_read32(bus,dev,0,0);
                if((id&0xFFFF)!=0x10EC||(id>>16)!=0x8139) continue;

                /* Bus master + IO enable */
                uint32_t cmd=pci_read32(bus,dev,0,0x04);
                pci_write32(bus,dev,0,0x04,cmd|0x05);

                /* IO base (BAR0) */
                io_base=(uint16_t)(pci_read32(bus,dev,0,0x10)&~3u);
                tx_idx=0;
                tx_buf=(uint8_t*)0x03000000;
                rx_buf=(uint8_t*)0x03006000;

                /* Power on */
                hal_outb(io_base+RTL_CONFIG1,0x00);

                /* Software reset */
                hal_outb(io_base+RTL_CMD,0x10);
                for(int i=0;i<1000000;i++) if(!(hal_inb(io_base+RTL_CMD)&0x10)) break;

                /* RX buffer: g_net.rx_buf */
                hal_outl(io_base+RTL_RBSTART,(uint32_t)(uintptr_t)g_net.rx_buf);

                /* Interrupt mask: RX OK + TX OK */
                hal_outw(io_base+RTL_IMR,0x0005);

                /* RX config: accept all + wrap */
                hal_outl(io_base+RTL_RCR,0x0000F70F);

                /* TX config: default */
                hal_outl(io_base+RTL_TCR,0x03000700);

                /* Enable RX+TX */
                hal_outb(io_base+RTL_CMD,0x0C);

                /* MAC adresini oku */
                for(int i=0;i<6;i++)
                    g_net.my_mac.b[i]=hal_inb(io_base+RTL_IDR0+i);

                ready=true;
                return true;
            }
        }
        ready=false;
        return false;
    }

    bool send(const void* data, uint16_t len){
        if(!ready) return false;
        if(len>1500) len=1500;
        /* TX buffer: g_net.tx_buf[tx_idx] */
        uint8_t* buf=tx_buf+(tx_idx&3)*1536;
        for(uint16_t i=0;i<len;i++) buf[i]=((const uint8_t*)data)[i];
        hal_outl(io_base+RTL_TSAD[tx_idx&3],(uint32_t)(uintptr_t)buf);
        hal_outl(io_base+RTL_TSD[tx_idx&3], len&0x1FFF);
        tx_idx=(tx_idx+1)&3;
        return true;
    }

    /* RX paketini oku — data pointer'ı buffer içine döndürür */
    uint8_t* recv(uint16_t& out_len){
        if(!ready) return nullptr;
        uint16_t isr=hal_inw(io_base+RTL_ISR);
        if(!(isr&0x01)) return nullptr;
        hal_outw(io_base+RTL_ISR,0x01); /* clear */

        /* Paket başlığı: 4 byte (status, length) */
        uint16_t rx_off=g_net.rx_read;
        uint8_t* hdr=g_net.rx_buf+(rx_off&(sizeof(g_net.rx_buf)-1));
        uint16_t pkt_len=*(uint16_t*)(hdr+2);
        if(pkt_len<14||pkt_len>1536){ g_net.rx_read+=4; return nullptr; }
        uint8_t* pkt=hdr+4;
        out_len=(uint16_t)(pkt_len-4); /* CRC hariç */
        g_net.rx_read=(uint16_t)((rx_off+4+pkt_len+3)&~3u);
        hal_outw(io_base+RTL_CAPR,(uint16_t)(g_net.rx_read-0x10));
        return pkt;
    }
};

extern RTL8139 g_rtl;

#endif
