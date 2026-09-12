#ifndef STARTUP_H
#define STARTUP_H
/*
 * startup.h — LightOS Boot Animation + Developer Mode
 *
 * Başlangıçta 5 saniyelik animasyon gösterir.
 * Bu sürede 5 kez F4 basılırsa Developer Mode ekranı açılır.
 *
 * Dev Mode aktifse:
 *   - Sistem dosyaları File Manager'da görünür
 *   - Reboot tetiklenir (Y seçilirse)
 */

#include "graphics.h"
#include "font.h"
#include "kernel/hal.h"
#include "kernel/acpi.h"

extern bool g_dev_mode;

static int t_strlen_su(const char* s){ int n=0; while(s[n]) n++; return n; }
static void ShowLoginScreen(); /* forward decl — RunStartup() sonunda çağrılır */

static void startup_outb(uint16_t p,uint8_t v){hal_outb(p,v);}
static uint8_t startup_inb(uint16_t p){return hal_inb(p);}

/* Büyük metin yardımcısı */

/* Basit sin/cos lookup (isin/icos) — 360 derece, 0-100 range */
static const int SU_SIN[60]={
     0, 10, 21, 31, 41, 50, 59, 67, 74, 81,
    86, 91, 95, 98,100,100,100, 98, 95, 91,
    86, 81, 74, 67, 59, 50, 41, 31, 21, 10,
     0,-10,-21,-31,-41,-50,-59,-67,-74,-81,
   -86,-91,-95,-98,-100,-100,-100,-98,-95,-91,
   -86,-81,-74,-67,-59,-50,-41,-31,-21,-10
};
static int su_isin(int deg){ return SU_SIN[((deg%360)+360)%360*60/360]; }
static int su_icos(int deg){ return -SU_SIN[((deg-90+360)%360)*60/360]; }

static void SU_Text(const char* msg,int bx,int by,int scale,uint32_t color){
    for(int bi=0;msg[bi];bi++){
        unsigned char uc=(unsigned char)msg[bi];
        if(uc<32||uc>127){bx+=9*scale;continue;}
        const unsigned char* g=font_ascii[uc-32];
        for(int row=0;row<8;row++){
            unsigned char bits=g[row];
            for(int col=0;col<8;col++)
                if(bits&(0x80>>col))
                    for(int sy=0;sy<scale;sy++)
                        for(int sx=0;sx<scale;sx++)
                            PutPixel(bx+col*scale+sx,by+row*scale+sy,color);
        }
        bx+=9*scale;
    }
}

static void SU_Wait(unsigned int n){
    for(volatile unsigned int i=0;i<n;i++) __asm__ volatile("nop");
}

/* Klavye tarama (polling — IRQ olmadan) */
static uint8_t SU_ScanKey(){
    if(!(startup_inb(0x64)&1)) return 0;
    return startup_inb(0x60);
}

/* Developer Mode onay ekranı */
static bool ShowDevModePrompt(){
    extern uint32_t* vram;
    extern uint32_t  screen_pitch;

    /* Ekranı siyah + koyu mavi çerçeve */
    for(int row=0;row<(int)SH;row++)
        DrawRect(0,row,SW,1,LerpColor(0x000000,0x000820,row,SH));

    /* Çerçeve */
    int bx=80,by=150,bw=640,bh=260;
    DrawGradientV(bx,by,bw,bh,0x0A0A20,0x101830);
    DrawRectBorder(bx,by,bw,bh,0xFF8800);
    DrawRectBorder(bx+2,by+2,bw-4,bh-4,0x804400);

    /* Uyarı ikon */
    SU_Text("!",bx+bw/2-8,by+18,3,0xFF8800);

    /* Başlık */
    SU_Text("DEVELOPER MODE",bx+70,by+60,2,0xFF8800);

    /* Alt çizgi */
    DrawRect(bx+20,by+82,bw-40,1,0xFF8800);

    /* Soru */
    SU_Text("Are you sure for opening",bx+55,by+96,1,0xDDDDDD);
    SU_Text("DEVELOPER MODE?",bx+115,by+112,1,0xFFCC44);

    /* Seçenekler */
    DrawGradientV(bx+60,by+145,120,30,0x204010,0x102008);
    DrawRectBorder(bx+60,by+145,120,30,0x44AA22);
    SU_Text("[Y] Yes",bx+80,by+154,1,0x88FF44);

    DrawGradientV(bx+bw-180,by+145,120,30,0x200808,0x100404);
    DrawRectBorder(bx+bw-180,by+145,120,30,0xAA2222);
    SU_Text("[N] No",bx+bw-162,by+154,1,0xFF8844);

    /* Uyarılar */
    DrawRect(bx+20,by+190,bw-40,1,0x443300);
    SU_Text("WARNING: System files will be",bx+130,by+202,1,0xFF6600);
    SU_Text("visible. Use with caution.",bx+155,by+216,1,0xFF6600);

    /* Geri sayım */
    char countdown_msg[]="Auto-cancel in: 10 seconds";

    /* Ekrana bas */
    {
        uint8_t* src=(uint8_t*)backbuffer;
        uint8_t* dst=(uint8_t*)vram;
        for(uint32_t y2=0;y2<SH;y2++){
            uint8_t* s=src+y2*SW*4;
            uint8_t* d=dst+y2*screen_pitch;
            for(uint32_t i=0;i<SW*4;i++) d[i]=s[i];
        }
    }

    /* RTC ile tam 10 saniye geri sayım */
    auto rtc_s2 = []()->uint8_t{
        uint8_t v;
        __asm__ volatile("outb %0,%1"::"a"((uint8_t)0x00),"Nd"((uint16_t)0x70));
        __asm__ volatile("inb %1,%0":"=a"(v):"Nd"((uint16_t)0x71));
        return (uint8_t)((v>>4)*10+(v&0xF));
    };
    uint8_t p_start = rtc_s2();
    int last_shown  = -1;

    while(1){
        uint8_t now2 = rtc_s2();
        int diff2 = (int)now2 - (int)p_start;
        if(diff2 < 0) diff2 += 60;
        int remaining = 10 - diff2;
        if(remaining < 0) return false; /* timeout */

        /* Saniye değişince ekranı güncelle */
        if(remaining != last_shown){
            last_shown = remaining;
            countdown_msg[16]='0'+remaining/10;
            countdown_msg[17]='0'+remaining%10;
            DrawRect(bx+20,by+238,bw-40,14,0x0A0A20);
            SU_Text(countdown_msg,bx+120,by+240,1,0x8888AA);
            /* Geri sayım bar */
            int bar_w2 = remaining * (bw-40) / 10;
            DrawRect(bx+20,by+252,bw-40,4,0x0A0A20);
            if(bar_w2>0) DrawGradientH(bx+20,by+252,bar_w2,4,0xFF8800,0xFFCC44);
            {
                uint8_t* src=(uint8_t*)backbuffer;
                uint8_t* dst=(uint8_t*)vram;
                for(uint32_t y2=0;y2<SH;y2++){
                    uint8_t* s=src+y2*SW*4;
                    uint8_t* d=dst+y2*screen_pitch;
                    for(uint32_t i=0;i<SW*4;i++) d[i]=s[i];
                }
            }
        }

        /* Tuş denetle */
        uint8_t sc=SU_ScanKey();
        if(sc==0x15) return true;  /* Y */
        if(sc==0x31) return false; /* N */
        SU_Wait(50000u);
    }
    return false;
}

/* Ana Startup Animasyonu */
static void RunStartup(){
/* VGA text debug: "BOOT" yaz */
{
    volatile uint16_t* vga=(volatile uint16_t*)0xB8000;
    vga[0]=0x2F42; vga[1]=0x2F4F; vga[2]=0x2F4F; vga[3]=0x2F54; /* BOOT green */
}

    extern uint32_t* vram;
    extern uint32_t  screen_pitch;

    int f4_count=0;
    bool dev_triggered=false;

    /* ── 5 saniyelik animasyon ── */
    /* RTC ile tam 5 saniye — her RTC tick'i oku */
    auto rtc_sec_now = []()->uint8_t{
        uint8_t v;
        __asm__ volatile("outb %0,%1"::"a"((uint8_t)0x00),"Nd"((uint16_t)0x70));
        __asm__ volatile("inb %1,%0":"=a"(v):"Nd"((uint16_t)0x71));
        return (uint8_t)((v>>4)*10+(v&0xF));
    };
    uint8_t start_sec = rtc_sec_now();
    int elapsed_sec   = 0;
    int sub_frame     = 0; /* 0-49 arası her saniyede */

    while(elapsed_sec < 5 && !dev_triggered){
        uint8_t now = rtc_sec_now();
        int diff = (int)now - (int)start_sec;
        if(diff < 0) diff += 60;
        if(diff > elapsed_sec){ elapsed_sec=diff; sub_frame=0; }
        int frame = elapsed_sec*50 + sub_frame;
        if(frame>249) frame=249;
        sub_frame++;

        /* Arkaplan gradient */
        for(int row=0;row<(int)SH;row++)
            DrawRect(0,row,SW,1,LerpColor(0x000000,0x050A14,row,SH));

        /* Logo halkası (5 saniyede tam büyür) */
        int progress = elapsed_sec*20 + sub_frame*20/50; /* 0-100 */
        int ring_r   = 20 + progress*60/100;
        for(int d=0;d<360;d+=2){
            int px2=SW/2+ring_r*su_isin(d)/100;
            int py2=SH/2+ring_r*su_icos(d)/100;
            uint32_t clr=LerpColor(0x204870,0x60C0FF,sub_frame%50,50);
            if(px2>=0&&py2>=0&&px2<(int)SW&&py2<(int)SH)
                PutPixel(px2,py2,clr);
        }

        /* "LightOS" — ilk saniyede beliriyor */
        if(elapsed_sec>=1||sub_frame>20){
            int alpha=elapsed_sec<2?(sub_frame>20?sub_frame-20:0):30;
            uint32_t tc=LerpColor(0x000000,0x80C8FF,alpha,30);
            SU_Text("LightOS",SW/2-7*14,SH/2-22,3,tc);
        }
        if(elapsed_sec>=2){
            SU_Text("2 Pro",SW/2-2*18,SH/2+28,2,0x4080AA);
        }

        /* Progress bar — tam 5 saniyeye göre */
        int bar_w = (elapsed_sec*200 + sub_frame*200/50)*440/1000;
        if(bar_w>440) bar_w=440;
        DrawRect(SW/2-220,SH-80,440,6,0x0A1020);
        DrawRectBorder(SW/2-220,SH-80,440,6,0x203050);
        if(bar_w>0) DrawGradientH(SW/2-220,SH-80,bar_w,6,0x2060A0,0x60C0FF);

        SU_Text("Starting...",SW/2-5*9,SH-64,1,0x506070);

        /* F4 sayacı */
        if(f4_count>0){
            char hint[]="F4 x?  (5 to unlock)";
            hint[4]='0'+f4_count;
            SU_Text(hint,SW/2-9*9,SH-48,1,0xFF8800);
        } else {
            SU_Text("Press F4 x5 for Dev Mode",SW/2-11*9,SH-48,1,0x303848);
        }

        /* Ekrana bas */
        {
            uint8_t* src=(uint8_t*)backbuffer;
            uint8_t* dst=(uint8_t*)vram;
            for(uint32_t y2=0;y2<SH;y2++){
                uint8_t* s=src+y2*SW*4;
                uint8_t* d=dst+y2*screen_pitch;
                for(uint32_t i=0;i<SW*4;i++) d[i]=s[i];
            }
        }

        /* ~20ms busy wait */
        SU_Wait(500000u);

        /* Klavye tara */
        uint8_t sc=SU_ScanKey();
        if(sc==0x3E){ f4_count++; if(f4_count>=5) dev_triggered=true; }
    }

    /* Developer Mode tetiklendi mi? */
    if(dev_triggered){
        bool confirmed=ShowDevModePrompt();
        /* Y: dev mode aktif, direkt devam et — reboot yok */
        g_dev_mode = confirmed;
    }

    /* ── Login ekranı ──
     * Boot animasyonundan sonra, masaüstüne geçmeden önce basit bir
     * kullanıcı seçim ekranı gösterilir. Mouse bu noktada henüz init
     * edilmediği için (PS/2 mouse kurulumu start_shell() içinde daha
     * sonra yapılıyor) seçim tamamen klavye ile: yukarı/aşağı ok
     * kullanıcı değiştirir, Enter giriş yapar. Seçilen kullanıcı adı
     * g_current_user'a yazılır; terminal'deki "whoami" komutu ve
     * ileride eklenebilecek başka özellikler bunu okuyabilir. */
    ShowLoginScreen();
}

static void ShowLoginScreen(){
    extern uint32_t* vram;
    extern uint32_t  screen_pitch;
    extern char g_current_user[32];

    struct LoginUser{ const char* name; uint32_t col; };
    static const LoginUser users[]={
        {"Admin", 0x4A90E2},
        {"Guest", 0x8896A4},
    };
    const int N_USERS=2;
    int selected=0;

    auto blit=[&](){
        uint8_t* src=(uint8_t*)backbuffer;
        uint8_t* dst=(uint8_t*)vram;
        for(uint32_t y2=0;y2<SH;y2++){
            uint8_t* s=src+y2*SW*4;
            uint8_t* d=dst+y2*screen_pitch;
            for(uint32_t i=0;i<SW*4;i++) d[i]=s[i];
        }
    };

    bool logged_in=false;
    while(!logged_in){
        /* Arkaplan — sakin, koyu gece-mavisi (Rain temasıyla da uyumlu bir dil) */
        for(int row=0;row<(int)SH;row++)
            DrawRect(0,row,SW,1,LerpColor(0x0A0E16,0x18202C,row,SH));

        SU_Text("LightOS",SW/2-7*14,SH/2-160,3,0x80C8FF);
        SU_Text("Select a user to sign in",SW/2-12*9,SH/2-100,1,0x6C7A8C);

        int card_w=180, card_h=140, gap=30;
        int total_w=N_USERS*card_w+(N_USERS-1)*gap;
        int start_x=SW/2-total_w/2;
        int card_y=SH/2-40;

        for(int i=0;i<N_USERS;i++){
            int cx=start_x+i*(card_w+gap);
            bool sel=(i==selected);
            uint32_t bg = sel ? LerpColor(users[i].col,0x000000,25,100) : 0x1C2530;
            DrawRect(cx,card_y,card_w,card_h,bg);
            DrawRectBorder(cx,card_y,card_w,card_h,sel?users[i].col:0x2C3644);
            if(sel) DrawRectBorder(cx-2,card_y-2,card_w+4,card_h+4,LerpColor(users[i].col,0xFFFFFF,30,100));
            /* Basit avatar: renkli daire yerine kare+baş silueti */
            int ax=cx+card_w/2, ay=card_y+44;
            DrawRect(ax-20,ay-20,40,40,LerpColor(users[i].col,0x000000,10,100));
            DrawRect(ax-12,ay-12,24,24,users[i].col);
            SU_Text(users[i].name,cx+card_w/2-(int)(t_strlen_su(users[i].name))*9/2,card_y+card_h-30,1,
                sel?0xFFFFFF:0xAAB4C0);
        }

        SU_Text("Left/Right: choose   Enter: sign in",SW/2-18*9,SH-40,1,0x505C6C);

        blit();
        SU_Wait(400000u);

        uint8_t sc=SU_ScanKey();
        if(sc==0x4B){ selected=(selected-1+N_USERS)%N_USERS; } /* Left */
        else if(sc==0x4D){ selected=(selected+1)%N_USERS; }    /* Right */
        else if(sc==0x1C){ /* Enter */
            int i=0; while(users[selected].name[i]&&i<31){ g_current_user[i]=users[selected].name[i]; i++; }
            g_current_user[i]=0;
            logged_in=true;
        }
    }
}

#endif
