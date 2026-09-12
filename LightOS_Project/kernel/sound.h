#ifndef SOUND_H
#define SOUND_H
/*
 * kernel/sound.h — LightOS AC97 + PC Speaker
 * ============================================
 * AC97: VirtualBox ICH AC97 + QEMU + gerçek donanım
 * Fallback: PC Speaker
 *
 * QEMU Windows: -device AC97,audiodev=snd0 -audiodev dsound,id=snd0
 * QEMU Linux:   -device AC97,audiodev=snd0 -audiodev pa,id=snd0
 * QEMU macOS:   -device AC97,audiodev=snd0 -audiodev coreaudio,id=snd0
 * VirtualBox:   Audio Controller = ICH AC97
 */
#include <stdint.h>

/* ── IO port yardımcıları ── */
static void     snd_outb(uint16_t p,uint8_t  v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static void     snd_outw(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static void     snd_outl(uint16_t p,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static uint8_t  snd_inb (uint16_t p){uint8_t  v;__asm__ volatile("inb  %1,%0":"=a"(v):"Nd"(p));return v;}
static uint16_t snd_inw (uint16_t p){uint16_t v;__asm__ volatile("inw  %1,%0":"=a"(v):"Nd"(p));return v;}
static uint32_t snd_inl (uint16_t p){uint32_t v;__asm__ volatile("inl  %1,%0":"=a"(v):"Nd"(p));return v;}

static void snd_wait(unsigned int n){
    for(volatile unsigned int i=0;i<n;i++) __asm__ volatile("nop");
}

/* ══════════════════════════════════════════════
   PCI
   ══════════════════════════════════════════════ */
static uint32_t pci_cfg_read(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off){
    snd_outl(0xCF8, 0x80000000u|((uint32_t)bus<<16)|((uint32_t)dev<<11)|
                    ((uint32_t)fn<<8)|(off&0xFC));
    return snd_inl(0xCFC);
}
static void pci_cfg_write(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off,uint32_t val){
    snd_outl(0xCF8, 0x80000000u|((uint32_t)bus<<16)|((uint32_t)dev<<11)|
                    ((uint32_t)fn<<8)|(off&0xFC));
    snd_outl(0xCFC, val);
}

/* AC97 Intel 82801AA/AB/BA/DB — VirtualBox varsayılan */
static const uint32_t AC97_IDS[]={
    0x24158086u, /* 82801AA  */
    0x24258086u, /* 82801AB  */
    0x24458086u, /* 82801BA  */
    0x24C58086u, /* 82801DB  */
    0x24D58086u, /* 82801EB  */
    0x27DE8086u, /* 82801G   */
    0x74451022u, /* AMD      */
    0x01B110DEu, /* nForce   */
    0
};

static bool     g_ac97_ok = false;
static uint16_t g_nam     = 0;    /* AC97 Mixer (codec) */
static uint16_t g_nabm    = 0;    /* Bus Master (DMA)   */
static uint8_t  g_pci_bus = 0;
static uint8_t  g_pci_dev = 0;

/* ── AC97 register offsets ── */
/* NAM (mixer) */
#define AC97_RESET     0x00
#define AC97_MASTER    0x02
#define AC97_HEADPHONE 0x04
#define AC97_PCM_VOL   0x18
#define AC97_EXT_ID    0x28
#define AC97_EXT_CTRL  0x2A
#define AC97_PCM_RATE  0x2C
/* NABM (bus master) — PCM Out channel = 0x10 */
#define NABM_BDBAR     0x10   /* Buffer Descriptor Base Address */
#define NABM_CIV       0x14   /* Current Index Value */
#define NABM_LVI       0x15   /* Last Valid Index */
#define NABM_SR        0x16   /* Status Register */
#define NABM_PICB      0x18   /* Position in current buffer */
#define NABM_PIV       0x1A   /* Prefetched Index Value */
#define NABM_CR        0x1B   /* Control Register */
#define NABM_GLOB_CNT  0x2C   /* Global Control */
#define NABM_GLOB_STA  0x30   /* Global Status */

/* ── Buffer Descriptor ── */
struct __attribute__((packed)) BDL_Entry {
    uint32_t addr;
    uint16_t samples;   /* PCM sample pair sayısı */
    uint16_t flags;     /* bit15=IOC */
};

/* 32 entry BDL — spec gereği 32-byte aligned */
__attribute__((aligned(8)))
static BDL_Entry g_bdl[32];

/* PCM buffer — 48000Hz stereo 16-bit, ~0.5s */
static const int SAMPLE_RATE = 48000;
static const int BUF_SAMPLES = 8192;
__attribute__((aligned(4)))
static int16_t g_pcm[BUF_SAMPLES * 2];  /* L+R interleaved */

/* ── AC97 bulma ve başlatma ── */
static bool ac97_init() {
    /* PCI bus tara */
    for(uint8_t bus=0;bus<8;bus++){
        for(uint8_t dev=0;dev<32;dev++){
            uint32_t id=pci_cfg_read(bus,dev,0,0x00);
            if(id==0xFFFFFFFFu) continue;
            /* VID:DID eşleş — AC97_IDS[]: (DID<<16)|VID formatında */
            bool found=false;
            for(int k=0;AC97_IDS[k];k++){
                uint16_t vid=(uint16_t)(AC97_IDS[k]&0xFFFF);
                uint16_t did=(uint16_t)(AC97_IDS[k]>>16);
                if((id&0xFFFF)==vid && (id>>16)==did){ found=true; break; }
            }
            if(!found) continue;

            /* BAR0 = NAM, BAR1 = NABM
               IO BAR: bit0=1 → addr = val & ~3
               MMIO BAR: bit0=0 → skip (we only handle IO) */
            uint32_t bar0 = pci_cfg_read(bus,dev,0,0x10);
            uint32_t bar1 = pci_cfg_read(bus,dev,0,0x14);
            /* Try IO first */
            if((bar0&1) && (bar1&1)){
                g_nam  = (uint16_t)(bar0 & 0xFFFCu);
                g_nabm = (uint16_t)(bar1 & 0xFFFCu);
            } else {
                /* BAR not ready — try enabling IO space and re-read */
                uint32_t cmd = pci_cfg_read(bus,dev,0,0x04);
                pci_cfg_write(bus,dev,0,0x04,(cmd&0xFFFF0000u)|0x05);
                /* Write 0xFFFF to size-probe */
                pci_cfg_write(bus,dev,0,0x10,0xFFFFFFFFu);
                pci_cfg_write(bus,dev,0,0x14,0xFFFFFFFFu);
                /* Some BIOS assigns default IO */
                g_nam  = 0xD100;  /* VirtualBox typical ICH AC97 NAM  */
                g_nabm = 0xD200;  /* VirtualBox typical ICH AC97 NABM */
                bar0 = pci_cfg_read(bus,dev,0,0x10);
                bar1 = pci_cfg_read(bus,dev,0,0x14);
                if(bar0&1) g_nam  = (uint16_t)(bar0 & 0xFFFCu);
                if(bar1&1) g_nabm = (uint16_t)(bar1 & 0xFFFCu);
            }
            if(!g_nam || !g_nabm) continue;
            g_pci_bus=bus; g_pci_dev=dev;

            /* PCI komut: IO Enable + Bus Master */
            uint32_t cmd = pci_cfg_read(bus,dev,0,0x04);
            pci_cfg_write(bus,dev,0,0x04,(cmd&0xFFFF0000u)|0x05);

            /* Global Reset */
            snd_outl(g_nabm+NABM_GLOB_CNT, 0x02);
            snd_wait(20000000u);
            snd_outl(g_nabm+NABM_GLOB_CNT, 0x00);
            snd_wait(20000000u);

            /* Codec cold reset */
            snd_outw(g_nam+AC97_RESET, 0x0000);
            snd_wait(30000000u);

            /* Codec ready bekle */
            for(int t=0;t<1000;t++){
                uint32_t sta=snd_inl(g_nabm+NABM_GLOB_STA);
                if(sta & (1<<8)) break; /* Primary Codec Ready */
                snd_wait(1000000u);
            }

            /* Volume ayarları — mute bitlerini temizle */
            snd_outw(g_nam+AC97_MASTER,    0x0000); /* Master: 0dB */
            snd_outw(g_nam+AC97_HEADPHONE, 0x0000); /* Headphone: 0dB */
            snd_outw(g_nam+AC97_PCM_VOL,   0x0000); /* PCM out: 0dB */
            snd_wait(5000000u);

            /* Sample rate: 48000 Hz */
            /* Extended Audio ID kontrolü */
            uint16_t ext_id = snd_inw(g_nam+AC97_EXT_ID);
            if(ext_id & 0x01){
                /* VRA destekleniyor */
                snd_outw(g_nam+AC97_EXT_CTRL, snd_inw(g_nam+AC97_EXT_CTRL)|0x01);
                snd_outw(g_nam+AC97_PCM_RATE, (uint16_t)SAMPLE_RATE);
            }
            /* VRA yoksa 48000Hz varsayılan */

            /* PCM Out kanalı reset */
            snd_outb(g_nabm+NABM_CR, 0x02);
            snd_wait(5000000u);
            snd_outb(g_nabm+NABM_CR, 0x00);

            g_ac97_ok=true;
            return true;
        }
    }
    return false;
}

/* ── Kare dalga PCM üret ── */
static int gen_square(uint32_t freq, int ms, int vol=6000) {
    int total = SAMPLE_RATE * ms / 1000;
    if(total > BUF_SAMPLES) total = BUF_SAMPLES;
    if(!freq){ for(int i=0;i<total;i++) g_pcm[i*2]=g_pcm[i*2+1]=0; return total; }
    uint32_t period = SAMPLE_RATE / freq;
    if(period < 2) period = 2;
    for(int i=0;i<total;i++){
        int16_t v = ((uint32_t)i % period < period/2) ? (int16_t)vol : (int16_t)-vol;
        g_pcm[i*2]=v; g_pcm[i*2+1]=v;
    }
    /* Fade-out son %10'da */
    int fade=total/10; if(fade<1)fade=1;
    for(int i=0;i<fade;i++){
        int16_t f=(int16_t)((int)g_pcm[(total-fade+i)*2]*(fade-i)/fade);
        g_pcm[(total-fade+i)*2]=f; g_pcm[(total-fade+i)*2+1]=f;
    }
    return total;
}

/* ── AC97 DMA oynatma ── */
static void ac97_play(int samples) {
    if(!samples) return;

    /* BDL kur: tek entry */
    g_bdl[0].addr    = (uint32_t)(uintptr_t)g_pcm;
    g_bdl[0].samples = (uint16_t)(samples);
    g_bdl[0].flags   = 0x8000; /* IOC */

    /* PCM Out DMA kanalını kur */
    snd_outl(g_nabm+NABM_BDBAR, (uint32_t)(uintptr_t)g_bdl);
    snd_outb(g_nabm+NABM_LVI,   0);    /* last valid = entry 0 */
    snd_outb(g_nabm+NABM_SR,    0x1C); /* status temizle */
    snd_outb(g_nabm+NABM_CR,    0x01); /* DMA başlat (RUN) */

    /* Tamamlanmayı bekle */
    for(int t=0;t<100000000;t++){
        uint16_t sr = snd_inw(g_nabm+NABM_SR);
        if(sr & 0x08) break;  /* BCIS: buffer completion */
        if(sr & 0x04) break;  /* LVBCI: last valid */
        __asm__ volatile("nop");
    }
    snd_outb(g_nabm+NABM_CR, 0x00); /* DMA durdur */
    snd_outw(g_nabm+NABM_SR, 0x1C); /* flag temizle */
}

/* ══════════════════════════════════════════════
   PC SPEAKER — fallback
   ══════════════════════════════════════════════ */
static const uint32_t PIT_BASE=1193182u;
static void spk_on(uint32_t f){
    uint32_t d=PIT_BASE/f;
    snd_outb(0x43,0xB6);
    snd_outb(0x42,(uint8_t)(d&0xFF));
    snd_outb(0x42,(uint8_t)(d>>8));
    snd_outb(0x61,snd_inb(0x61)|0x03);
}
static void spk_off(){ snd_outb(0x61,snd_inb(0x61)&~0x03); }
static void spk_ms(int ms){ for(volatile int i=0;i<ms*6000;i++) __asm__ volatile("nop"); }

/* ══════════════════════════════════════════════
   NOTA API
   ══════════════════════════════════════════════ */
struct Note { uint32_t freq; int ms; };
#define EGH  90
#define QRT  180
#define HLF  360
#define DOT(x) ((x)*3/2)
#define C4 262u
#define E4 330u
#define G4 392u
#define A4 440u
#define C5 523u
#define D5 587u
#define E5 659u
#define G5 784u

static void sound_init(){
    if(!g_ac97_ok) ac97_init();
}

static void play_note(uint32_t freq, int ms){
    if(g_ac97_ok){
        int s=gen_square(freq,ms);
        ac97_play(s);
    } else {
        if(freq) spk_on(freq); else spk_off();
        spk_ms(ms);
        spk_off();
        spk_ms(25);
    }
}

static void spk_play(const Note* n,int cnt){
    sound_init();
    for(int i=0;i<cnt;i++) play_note(n[i].freq,n[i].ms);
    spk_off();
}

static const Note STARTUP_MELODY[]={
    {E4,EGH},{G4,EGH},{C5,EGH},{E5,QRT},
    {0,60},{D5,EGH},{E5,EGH},{G5,QRT},
    {0,60},{E5,EGH},{C5,EGH},{A4,EGH},{G4,DOT(QRT)},
};
static const Note SHUTDOWN_MELODY[]={
    {G5,EGH},{E5,EGH},{C5,EGH},{G4,QRT},
    {0,60},{A4,EGH},{G4,EGH},{E4,HLF},
};
static const Note BSOD_SOUND[]={{110,1000}};

static void PlayStartup()  { spk_play(STARTUP_MELODY,13); }
static void PlayShutdown() { spk_play(SHUTDOWN_MELODY,8); }
static void PlayBsod()     { spk_play(BSOD_SOUND,1);      }

#endif
