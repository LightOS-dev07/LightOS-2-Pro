#ifndef BSOD_H
#define BSOD_H

/*
 * bsod.h — LightOS Blue Screen of Death
 * =======================================
 * Hata kodları ve BSOD ekranı.
 * DoBSOD() çağrılınca sistem durur (cli+hlt).
 */

#include "graphics.h"
#include "font.h"
#include "kernel/sound.h"
/* BSOD extern globals */
extern uint32_t* backbuffer;
extern uint32_t* vram;
extern uint32_t  screen_pitch;
extern uint32_t  screen_w;
extern uint32_t  screen_h;
static void BsodSwap(){
    uint8_t* src=(uint8_t*)backbuffer;
    uint8_t* dst=(uint8_t*)vram;
    uint32_t copy_w=(screen_w<SW?screen_w:(uint32_t)SW);
    for(uint32_t y=0;y<screen_h&&y<(uint32_t)SH;y++){
        uint8_t* s=src+y*(uint32_t)(SW*4);
        uint8_t* d=dst+y*screen_pitch;
        for(uint32_t x=0;x<copy_w*4;x++) d[x]=s[x];
    }
}


enum BsodCode {
    BSOD_GENERIC            = 0x00000000,
    BSOD_PAGE_FAULT         = 0x0000000E,
    BSOD_DIVIDE_BY_ZERO     = 0x00000000,
    BSOD_STACK_OVERFLOW     = 0x0000007E,
    BSOD_KERNEL_PANIC       = 0x000000FE,
    BSOD_DRIVER_IRQL        = 0x000000D1,
    BSOD_MEMORY_MANAGEMENT  = 0x0000001A,
    BSOD_IRQL_NOT_LESS_EQUAL= 0x0000000A,
    BSOD_UNEXPECTED_KERNEL  = 0x0000007F,
};

static const char* BsodCodeName(BsodCode code) {
    switch(code) {
        case 0x0000000E: return "PAGE_FAULT_IN_NONPAGED_AREA";
        case 0x0000007E: return "SYSTEM_THREAD_EXCEPTION_NOT_HANDLED";
        case 0x000000FE: return "BUGCODE_USB_DRIVER";
        case 0x000000D1: return "DRIVER_IRQL_NOT_LESS_OR_EQUAL";
        case 0x0000001A: return "MEMORY_MANAGEMENT";
        case 0x0000000A: return "IRQL_NOT_LESS_OR_EQUAL";
        case 0x0000007F: return "UNEXPECTED_KERNEL_MODE_TRAP";
        default:         return "KERNEL_ERROR";
    }
}

/* Yardımcı: int → hex string */
static void bsod_hex(uint32_t v, char* buf) {
    const char* hx = "0123456789ABCDEF";
    buf[0]='0'; buf[1]='x';
    for(int i=0;i<8;i++) buf[2+i]=hx[(v>>(28-i*4))&0xF];
    buf[10]=0;
}

/* Büyük metin çizici (2× scale) */
static void BsodText2x(const char* msg, int bx, int by, uint32_t color) {
    for(int bi=0;msg[bi];bi++){
        unsigned char uc=(unsigned char)msg[bi];
        if(uc<32||uc>127){bx+=18;continue;}
        const unsigned char* g=font_ascii[uc-32];
        for(int row=0;row<8;row++){
            unsigned char bits=g[row];
            for(int col=0;col<8;col++){
                if(bits&(0x80>>col)){
                    PutPixel(bx+col*2,   by+row*2,   color);
                    PutPixel(bx+col*2+1, by+row*2,   color);
                    PutPixel(bx+col*2,   by+row*2+1, color);
                    PutPixel(bx+col*2+1, by+row*2+1, color);
                }
            }
        }
        bx+=18;
    }
}

/* Büyük metin çizici (3× scale) */
static void BsodText3x(const char* msg, int bx, int by, uint32_t color) {
    for(int bi=0;msg[bi];bi++){
        unsigned char uc=(unsigned char)msg[bi];
        if(uc<32||uc>127){bx+=27;continue;}
        const unsigned char* g=font_ascii[uc-32];
        for(int row=0;row<8;row++){
            unsigned char bits=g[row];
            for(int col=0;col<8;col++){
                if(bits&(0x80>>col)){
                    for(int sy=0;sy<3;sy++)
                        for(int sx=0;sx<3;sx++)
                            PutPixel(bx+col*3+sx, by+row*3+sy, color);
                }
            }
        }
        bx+=27;
    }
}

/* ─────────────────────────────────────────────────────────────────
   DoBSOD — Ekranı mavi yap, hata mesajı göster, sistemi durdur
   ───────────────────────────────────────────────────────────────── */
__attribute__((noreturn))
static void DoBSOD(BsodCode code, const char* file, int line) {
    /* 1. Tam mavi zemin */
    const uint32_t BG = 0x0000AA;   /* klasik BSOD mavisi */
    for(int y=0;y<(int)SH;y++)
        for(int x=0;x<(int)SW;x++)
            backbuffer[y*SW+x]=BG;

    /* 2. Üst ve alt beyaz şerit */
    for(int x=0;x<(int)SW;x++){
        backbuffer[0*SW+x]=0xFFFFFF;
        backbuffer[1*SW+x]=0xFFFFFF;
        backbuffer[(SH-1)*SW+x]=0xFFFFFF;
        backbuffer[(SH-2)*SW+x]=0xFFFFFF;
    }

    /* 3. ":(  " emoji büyük */
    BsodText3x(":( ", 40, 40, 0xFFFFFF);

    /* 4. Ana mesaj */
    const char* main_msg = "Your LightOS ran into a problem.";
    BsodText2x(main_msg, 40, 110, 0xFFFFFF);

    /* 5. Hata kodu adı */
    const char* code_name = BsodCodeName(code);
    DrawString(40, 160, "Stop code:", 0xFFFFFF);
    /* Kod adını kırmızı-ish beyazla vurgula */
    int cx=40+StringWidth("Stop code: "); (void)cx;
    DrawString(40+StringWidth("Stop code: "), 160, code_name, 0xFFFFBB);

    /* 6. Hex kodu */
    char hexbuf[12]; bsod_hex((uint32_t)code, hexbuf);
    DrawString(40, 174, hexbuf, 0xCCCCCC);

    /* 7. Dosya ve satır */
    DrawString(40, 192, "Location:", 0xCCCCCC);
    if(file) DrawString(40+StringWidth("Location: "), 192, file, 0xFFFFBB);
    /* Satır numarasını yaz */
    char linebuf[8]; {
        int v=line; char tmp[8]; int i=0;
        if(v==0){tmp[i++]='0';}
        while(v>0){tmp[i++]='0'+v%10;v/=10;}
        int j=0; for(int k=i-1;k>=0;k--)linebuf[j++]=tmp[k]; linebuf[j]=0;
    }
    DrawString(40, 206, "Line:", 0xCCCCCC);
    DrawString(40+StringWidth("Line: "), 206, linebuf, 0xFFFFBB);

    /* 8. Alt bilgi */
    DrawString(40, SH-60,
        "If you want to learn more, please search online for:",
        0xCCCCCC);
    DrawString(40, SH-48, BsodCodeName(code), 0xFFFFBB);
    DrawString(40, SH-32, "LightOS 2 Pro  |  Xaef BTL  |  2026", 0x8888CC);

    /* 9. Ekrana bas (direkt VRAM) */
    {
        extern uint32_t* vram;
        extern uint32_t  screen_pitch;
        uint8_t* src=(uint8_t*)backbuffer;
        uint8_t* dst=(uint8_t*)vram;
        for(uint32_t y2=0;y2<SH;y2++){
            uint8_t* s=src+y2*SW*4;
            uint8_t* d=dst+y2*screen_pitch;
            for(uint32_t i=0;i<SW*4;i++) d[i]=s[i];
        }
    }

    /* 10. BSOD bip sesi */
    PlayBsod();
    /* 11. RTC tabanlı 10 saniye geri sayım */
    DrawString(SW/2-80,SH-60,"Restarting in 10 seconds...",0xAAAAAA);
    BsodSwap();
    {
        /* RTC saniye okuma — CMOS port 0x70/0x71 */
        auto rtc_sec_bcd=[]()->uint8_t{
            uint8_t v;
            __asm__ volatile("outb %0,%1"::"a"((uint8_t)0x00),"Nd"((uint16_t)0x70));
            __asm__ volatile("inb %1,%0":"=a"(v):"Nd"((uint16_t)0x71));
            return (uint8_t)((v>>4)*10+(v&0xF)); /* BCD→decimal */
        };
        uint8_t start_sec = rtc_sec_bcd();
        uint8_t last_sec  = start_sec;
        int     remaining = 10;
        /* İlk sayıyı çiz */
        char buf[4]; buf[0]=(char)('0'+remaining); buf[1]=0;
        DrawRect(SW/2+72,SH-64,16,14,0x0000AA);
        DrawString(SW/2+72,SH-60,buf,0xFFFFFF);
        BsodSwap();
        while(remaining > 0){
            uint8_t cur_sec = rtc_sec_bcd();
            /* Saniye değişti mi? (59→0 wrap'i de yakala) */
            if(cur_sec != last_sec){
                last_sec = cur_sec;
                remaining--;
                char b[4];
                b[0] = remaining>0 ? (char)('0'+remaining) : '0';
                b[1] = 0;
                DrawRect(SW/2+72,SH-64,16,14,0x0000AA);
                DrawString(SW/2+72,SH-60,b,0xFFFFFF);
                BsodSwap();
            }
        }
    }
    /* Keyboard controller reset (0x64/0xFE) — evrensel restart */
    __asm__ volatile("cli");
    for(volatile int _w=0;_w<100000;_w++);
    /* KBC reset pulse */
    uint8_t _s=0xFF;
    while(_s&0x02){ __asm__ volatile("inb $0x64,%0":"=a"(_s)); }
    __asm__ volatile("outb %0,$0x64"::"a"((uint8_t)0xFE));
    /* Fallback: hlt loop */
    while(1) __asm__ volatile("hlt");
}

/* Kolaylık makrosu */
#define BSOD(code) DoBSOD(code, __FILE__, __LINE__)

#endif
