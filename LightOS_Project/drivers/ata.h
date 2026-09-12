#ifndef ATA_H
#define ATA_H
/*
 * drivers/ata.h — ATA/IDE PIO Disk Sürücüsü
 * ============================================
 * Primary ATA: 0x1F0-0x1F7, IRQ 14
 * Secondary:   0x170-0x177, IRQ 15
 * PIO mode (no DMA — bare-metal safe)
 * LBA28: 128GB'a kadar disk desteği
 * QEMU: -drive file=disk.img,format=raw
 */
#include <stdint.h>

/* Portlar */
#define ATA_DATA(b)     ((b)+0)
#define ATA_ERR(b)      ((b)+1)
#define ATA_SECT_CNT(b) ((b)+2)
#define ATA_LBA_LO(b)   ((b)+3)
#define ATA_LBA_MID(b)  ((b)+4)
#define ATA_LBA_HI(b)   ((b)+5)
#define ATA_DRIVE(b)    ((b)+6)
#define ATA_STATUS(b)   ((b)+7)
#define ATA_CMD(b)      ((b)+7)
#define ATA_CTRL(b)     ((b)+0x206)

/* Status bitleri */
#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

/* Komutlar */
#define ATA_CMD_READ  0x20
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_ID    0xEC
#define ATA_CMD_FLUSH 0xE7

static inline void ata_outb(uint16_t p,uint8_t  v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline void ata_outw(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t  ata_inb(uint16_t p){uint8_t  v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline uint16_t ata_inw(uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}

/* 400ns gecikme (status'u 4 kere okuma) */
static void ata_delay(uint16_t base){
    ata_inb(ATA_STATUS(base)); ata_inb(ATA_STATUS(base));
    ata_inb(ATA_STATUS(base)); ata_inb(ATA_STATUS(base));
}

/* BSY temizlenene kadar bekle */
static bool ata_wait_busy(uint16_t base, int timeout=1000000){
    for(int i=0;i<timeout;i++){
        uint8_t s=ata_inb(ATA_STATUS(base));
        if(!(s&ATA_SR_BSY)) return true;
    }
    return false;
}

/* DRQ veya ERR bekle */
static bool ata_wait_drq(uint16_t base){
    for(int i=0;i<1000000;i++){
        uint8_t s=ata_inb(ATA_STATUS(base));
        if(s&ATA_SR_ERR) return false;
        if(s&ATA_SR_DRQ) return true;
    }
    return false;
}

struct ATADrive {
    uint16_t base;
    uint8_t  drive;    /* 0=master 1=slave */
    bool     present;
    uint32_t sectors;  /* toplam 512-byte sektör sayısı */
    char     model[41];

    bool init(uint16_t _base, uint8_t _drive){
        base=_base; drive=_drive; present=false; sectors=0; model[0]=0;

        /* Soft reset */
        ata_outb(ATA_CTRL(base), 0x04);
        for(volatile int i=0;i<10000;i++);
        ata_outb(ATA_CTRL(base), 0x00);

        /* Drive seç */
        ata_outb(ATA_DRIVE(base), 0xA0|(drive<<4));
        ata_delay(base);

        /* IDENTIFY */
        ata_outb(ATA_SECT_CNT(base), 0);
        ata_outb(ATA_LBA_LO(base),   0);
        ata_outb(ATA_LBA_MID(base),  0);
        ata_outb(ATA_LBA_HI(base),   0);
        ata_outb(ATA_CMD(base),      ATA_CMD_ID);

        /* Drive var mı? */
        uint8_t s=ata_inb(ATA_STATUS(base));
        if(s==0) return false;

        if(!ata_wait_busy(base)) return false;

        /* ATAPI değil mi? */
        if(ata_inb(ATA_LBA_MID(base))||ata_inb(ATA_LBA_HI(base))) return false;

        if(!ata_wait_drq(base)) return false;

        /* 256 word oku */
        uint16_t id[256];
        for(int i=0;i<256;i++) id[i]=ata_inw(ATA_DATA(base));

        /* Model adı: word 27-46 */
        int mi=0;
        for(int w=27;w<=46;w++){
            model[mi++]=(char)(id[w]>>8);
            model[mi++]=(char)(id[w]&0xFF);
        }
        model[40]=0;
        /* Sondaki boşlukları kırp */
        for(int i=39;i>=0&&model[i]==' ';i--) model[i]=0;

        /* LBA28 sektör sayısı: word 60-61 */
        sectors=((uint32_t)id[61]<<16)|id[60];
        if(!sectors) return false;

        present=true;
        return true;
    }

    /* Tek sektör oku (512 byte) → buf */
    bool read_sector(uint32_t lba, uint8_t* buf){
        if(!present||lba>=sectors) return false;

        ata_wait_busy(base);
        ata_outb(ATA_DRIVE(base),    0xE0|(drive<<4)|((lba>>24)&0x0F));
        ata_outb(ATA_SECT_CNT(base), 1);
        ata_outb(ATA_LBA_LO(base),   (uint8_t)(lba));
        ata_outb(ATA_LBA_MID(base),  (uint8_t)(lba>>8));
        ata_outb(ATA_LBA_HI(base),   (uint8_t)(lba>>16));
        ata_outb(ATA_CMD(base),      ATA_CMD_READ);

        if(!ata_wait_busy(base)||!ata_wait_drq(base)) return false;
        if(ata_inb(ATA_STATUS(base))&ATA_SR_ERR) return false;

        for(int i=0;i<256;i++){
            uint16_t w=ata_inw(ATA_DATA(base));
            buf[i*2]=(uint8_t)w;
            buf[i*2+1]=(uint8_t)(w>>8);
        }
        return true;
    }

    /* Tek sektör yaz (512 byte) ← buf */
    bool write_sector(uint32_t lba, const uint8_t* buf){
        if(!present||lba>=sectors) return false;

        ata_wait_busy(base);
        ata_outb(ATA_DRIVE(base),    0xE0|(drive<<4)|((lba>>24)&0x0F));
        ata_outb(ATA_SECT_CNT(base), 1);
        ata_outb(ATA_LBA_LO(base),   (uint8_t)(lba));
        ata_outb(ATA_LBA_MID(base),  (uint8_t)(lba>>8));
        ata_outb(ATA_LBA_HI(base),   (uint8_t)(lba>>16));
        ata_outb(ATA_CMD(base),      ATA_CMD_WRITE);

        if(!ata_wait_busy(base)||!ata_wait_drq(base)) return false;

        for(int i=0;i<256;i++)
            ata_outw(ATA_DATA(base),(uint16_t)(buf[i*2]|(buf[i*2+1]<<8)));

        ata_outb(ATA_CMD(base), ATA_CMD_FLUSH);
        ata_wait_busy(base);
        return !(ata_inb(ATA_STATUS(base))&ATA_SR_ERR);
    }

    /* N sektör oku */
    bool read(uint32_t lba, uint32_t count, uint8_t* buf){
        for(uint32_t i=0;i<count;i++){
            if(!read_sector(lba+i, buf+i*512)) return false;
        }
        return true;
    }

    /* N sektör yaz */
    bool write(uint32_t lba, uint32_t count, const uint8_t* buf){
        for(uint32_t i=0;i<count;i++){
            if(!write_sector(lba+i, buf+i*512)) return false;
        }
        return true;
    }
};

/* Global disk instance
 * NOT: Bu header birden fazla .cpp dosyasına include edilebiliyor
 * (shell.cpp, notepad.cpp, luigi.h ...). "inline" olmadan gerçek bir
 * tanım koymak her include eden .o dosyasında ayrı bir g_disk yaratır
 * ve linkerda "multiple definition" hatası verir. C++17 inline variable
 * ile tek bir örnek garanti edilir, extra bir .cpp'ye taşımaya gerek kalmaz. */
inline ATADrive g_disk;

static bool disk_init(){
    if(g_disk.init(0x1F0, 0)) return true;  /* Primary master */
    if(g_disk.init(0x1F0, 1)) return true;  /* Primary slave  */
    if(g_disk.init(0x170, 0)) return true;  /* Secondary master */
    return false;
}
#endif
