#ifndef E1000_H
#define E1000_H
/*
 * kernel/net/e1000.h — Intel Pro/1000 T Server (82543GC) Driver
 * ==============================================================
 * PCI ID: 0x8086:0x1004 (82543GC)
 * QEMU:   -device e1000,netdev=n0 -netdev user,id=n0
 * VBox:   Intel PRO/1000 MT Server
 * 
 * MMIO tabanlı — BARdan register adresi okunur.
 */
#include "net.h"
#include "../hal.h"

/* E1000 Register Offsets */
#define E1000_CTRL   0x0000  /* Device Control */
#define E1000_STATUS 0x0008  /* Device Status */
#define E1000_EERD   0x0014  /* EEPROM Read */
#define E1000_RCTL   0x0100  /* RX Control */
#define E1000_TCTL   0x0400  /* TX Control */
#define E1000_RDBAL  0x2800  /* RX Desc Base Low */
#define E1000_RDBAH  0x2804  /* RX Desc Base High */
#define E1000_RDLEN  0x2808  /* RX Desc Length */
#define E1000_RDH    0x2810  /* RX Desc Head */
#define E1000_RDT    0x2818  /* RX Desc Tail */
#define E1000_TDBAL  0x3800  /* TX Desc Base Low */
#define E1000_TDBAH  0x3804  /* TX Desc Base High */
#define E1000_TDLEN  0x3808  /* TX Desc Length */
#define E1000_TDH    0x3810  /* TX Desc Head */
#define E1000_TDT    0x3818  /* TX Desc Tail */
#define E1000_MTA    0x5200  /* Multicast Table (128 regs) */
#define E1000_RAL    0x5400  /* RX Address Low */
#define E1000_RAH    0x5404  /* RX Address High */

/* CTRL bits */
#define E1000_CTRL_RST    (1<<26)
#define E1000_CTRL_SLU    (1<<6)
#define E1000_CTRL_ASDE   (1<<5)

/* RCTL bits */
#define E1000_RCTL_EN     (1<<1)
#define E1000_RCTL_SBP    (1<<2)
#define E1000_RCTL_UPE    (1<<3)
#define E1000_RCTL_MPE    (1<<4)
#define E1000_RCTL_BAM    (1<<15)
#define E1000_RCTL_BSIZE_2048 0
#define E1000_RCTL_SECRC  (1<<26)

/* TCTL bits */
#define E1000_TCTL_EN     (1<<1)
#define E1000_TCTL_PSP    (1<<3)
#define E1000_TCTL_CT_DEF (0x10<<4)
#define E1000_TCTL_COLD   (0x40<<12)

/* RX Descriptor */
struct __attribute__((packed)) E1000_RXDesc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
};

/* TX Descriptor */
struct __attribute__((packed)) E1000_TXDesc {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
};

static const int E1000_RX_DESC = 32;
static const int E1000_TX_DESC = 8;
static const int E1000_RX_BUF  = 2048;

/* PCI helpers */
static uint32_t e1000_pci_read(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t reg){
    hal_outl(0xCF8,0x80000000u|((uint32_t)bus<<16)|((uint32_t)dev<<11)|((uint32_t)fn<<8)|(reg&0xFC));
    return hal_inl(0xCFC);
}
static void e1000_pci_write(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t reg,uint32_t v){
    hal_outl(0xCF8,0x80000000u|((uint32_t)bus<<16)|((uint32_t)dev<<11)|((uint32_t)fn<<8)|(reg&0xFC));
    hal_outl(0xCFC,v);
}

struct E1000 {
    volatile uint32_t* mmio;   /* MMIO base */
    bool               ready;
    uint8_t            rx_idx;
    uint8_t            tx_idx;

    E1000_RXDesc* rx_desc; /* 0x03010000 */
    E1000_TXDesc* tx_desc; /* 0x03011000 */
    uint8_t* rx_buf;        /* 0x03012000, 32x2048 */
    uint8_t* tx_buf;        /* 0x03022000, 8x1536  */

    uint32_t reg_read(uint32_t off){ return mmio[off/4]; }
    void     reg_write(uint32_t off,uint32_t v){ mmio[off/4]=v; }

    /* EEPROM okuma — MAC adresi için */
    uint16_t eeprom_read(uint8_t addr){
        reg_write(E1000_EERD,(uint32_t)(addr<<8)|1);
        uint32_t v=0;
        for(int i=0;i<10000;i++){v=reg_read(E1000_EERD);if(v&(1<<4))break;}
        return (uint16_t)(v>>16);
    }

    bool init(){
        /* PCI scan: 82543GC + diğer e1000 varyantları */
        struct { uint16_t vid,did; } ids[]={
            {0x8086,0x1004}, /* 82543GC Server — QEMU default */
            {0x8086,0x100E}, /* 82540EM — QEMU e1000 */
            {0x8086,0x1015}, /* 82541GI */
            {0x8086,0x1026}, /* 82545GM */
            {0x8086,0x107C}, /* 82541PI */
            {0,0}
        };

        for(uint8_t bus=0;bus<16;bus++){
            for(uint8_t dev=0;dev<32;dev++){
                uint32_t id=e1000_pci_read(bus,dev,0,0);
                if(id==0xFFFFFFFF) continue;
                uint16_t vid=(uint16_t)(id&0xFFFF);
                uint16_t did=(uint16_t)(id>>16);
                bool found=false;
                for(int k=0;ids[k].vid;k++)
                    if(ids[k].vid==vid&&ids[k].did==did){found=true;break;}
                if(!found) continue;

                /* BAR0: MMIO */
                uint32_t bar0=e1000_pci_read(bus,dev,0,0x10)&~0xF;
                if(!bar0) continue;
                mmio=(volatile uint32_t*)(uintptr_t)bar0;

                /* Bus Master + Memory Enable */
                uint32_t cmd=e1000_pci_read(bus,dev,0,0x04);
                e1000_pci_write(bus,dev,0,0x04,cmd|0x06);

                /* Reset */
                reg_write(E1000_CTRL,reg_read(E1000_CTRL)|E1000_CTRL_RST);
                for(volatile int i=0;i<100000;i++);
                /* Auto-speed, link up */
                reg_write(E1000_CTRL,E1000_CTRL_ASDE|E1000_CTRL_SLU);

                /* MAC: EEPROM'dan oku */
                uint16_t mac0=eeprom_read(0);
                uint16_t mac1=eeprom_read(1);
                uint16_t mac2=eeprom_read(2);
                g_net.my_mac.b[0]=mac0&0xFF; g_net.my_mac.b[1]=mac0>>8;
                g_net.my_mac.b[2]=mac1&0xFF; g_net.my_mac.b[3]=mac1>>8;
                g_net.my_mac.b[4]=mac2&0xFF; g_net.my_mac.b[5]=mac2>>8;

                /* Multicast tabloyu temizle */
                for(int i=0;i<128;i++) reg_write(E1000_MTA+i*4,0);

                /* RX descriptor ring kur */
                for(int i=0;i<E1000_RX_DESC;i++){
                    rx_desc[i].addr=(uint64_t)(uint32_t)rx_buf[i];
                    rx_desc[i].status=0;
                }
                reg_write(E1000_RDBAL,(uint32_t)(uintptr_t)rx_desc);
                reg_write(E1000_RDBAH,0);
                reg_write(E1000_RDLEN,E1000_RX_DESC*16);
                reg_write(E1000_RDH,0);
                reg_write(E1000_RDT,E1000_RX_DESC-1);
                reg_write(E1000_RCTL,
                    E1000_RCTL_EN|E1000_RCTL_SBP|E1000_RCTL_UPE|
                    E1000_RCTL_MPE|E1000_RCTL_BAM|E1000_RCTL_SECRC|
                    E1000_RCTL_BSIZE_2048);

                /* TX descriptor ring kur */
                for(int i=0;i<E1000_TX_DESC;i++) tx_desc[i].status=0xFF;
                reg_write(E1000_TDBAL,(uint32_t)(uintptr_t)tx_desc);
                reg_write(E1000_TDBAH,0);
                reg_write(E1000_TDLEN,E1000_TX_DESC*16);
                reg_write(E1000_TDH,0);
                reg_write(E1000_TDT,0);
                reg_write(E1000_TCTL,
                    E1000_TCTL_EN|E1000_TCTL_PSP|
                    E1000_TCTL_CT_DEF|E1000_TCTL_COLD);

                /* RAL/RAH: MAC adresini NIC'e yaz */
                uint32_t ral=(uint32_t)g_net.my_mac.b[0]|
                             ((uint32_t)g_net.my_mac.b[1]<<8)|
                             ((uint32_t)g_net.my_mac.b[2]<<16)|
                             ((uint32_t)g_net.my_mac.b[3]<<24);
                uint32_t rah=(uint32_t)g_net.my_mac.b[4]|
                             ((uint32_t)g_net.my_mac.b[5]<<8)|(1u<<31);
                reg_write(E1000_RAL,ral);
                reg_write(E1000_RAH,rah);

                /* Sabit adres NIC DMA buffer'ları */
                rx_desc=(E1000_RXDesc*)0x03010000;
                tx_desc=(E1000_TXDesc*)0x03011000;
                rx_buf=(uint8_t*)0x03012000;
                tx_buf=(uint8_t*)0x03022000;
                for(int i=0;i<E1000_RX_DESC;i++){
                    rx_desc[i].addr=(uint64_t)(uintptr_t)(rx_buf+i*E1000_RX_BUF);
                    rx_desc[i].status=0;
                }
                for(int i=0;i<E1000_TX_DESC;i++) tx_desc[i].status=0xFF;
                reg_write(E1000_RDBAL,(uint32_t)(uintptr_t)rx_desc);
                reg_write(E1000_RDBAH,0);
                reg_write(E1000_RDLEN,E1000_RX_DESC*16);
                reg_write(E1000_RDH,0);
                reg_write(E1000_RDT,E1000_RX_DESC-1);
                reg_write(E1000_TDBAL,(uint32_t)(uintptr_t)tx_desc);
                reg_write(E1000_TDBAH,0);
                reg_write(E1000_TDLEN,E1000_TX_DESC*16);
                reg_write(E1000_TDH,0);
                reg_write(E1000_TDT,0);
                rx_idx=0; tx_idx=0;
                ready=true;
                return true;
            }
        }
        ready=false;
        return false;
    }

    bool send(const void* data,uint16_t len){
        if(!ready||len>1500) return false;
        uint8_t* buf=tx_buf+tx_idx*1536;
        for(uint16_t i=0;i<len;i++) buf[i]=((const uint8_t*)data)[i];
        tx_desc[tx_idx].addr  =(uint64_t)(uintptr_t)buf;
        tx_desc[tx_idx].length=len;
        tx_desc[tx_idx].cmd   =0x0B; /* EOP+IFCS+RS */
        tx_desc[tx_idx].status=0;
        uint8_t old_idx=tx_idx;
        tx_idx=(tx_idx+1)%E1000_TX_DESC;
        reg_write(E1000_TDT,tx_idx);
        /* TX tamamlanmasını bekle */
        for(int t=0;t<100000;t++)
            if(tx_desc[old_idx].status&0x01) break;
        return true;
    }

    uint8_t* recv(uint16_t& out_len){
        if(!ready) return nullptr;
        if(!(rx_desc[rx_idx].status&0x01)) return nullptr;
        out_len=rx_desc[rx_idx].length;
        uint8_t* pkt=rx_buf+rx_idx*E1000_RX_BUF;
        rx_desc[rx_idx].status=0;
        reg_write(E1000_RDT,rx_idx);
        rx_idx=(rx_idx+1)%E1000_RX_DESC;
        return pkt;
    }
};

extern E1000 g_e1000;

#endif
