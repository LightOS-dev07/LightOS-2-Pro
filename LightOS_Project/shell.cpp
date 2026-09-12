/* LightOS 2 Pro — Shell / GUI */
#include "shared.h"
#include "graphics.h"
#include "bsod.h"
#include "gui.h"
#include "lang.h"
#include "theme.h"
#include "compositor.h"
#include "wallpaper.h"
#include "kernel/hal.h"
#include "kernel/acpi.h"
#include "kernel/vfs.h"
#include "kernel/clock.h"
#include "drivers/serial.h"
#include "kernel/net/tcp.h"
#include "kernel/net/e1000.h"
#include "kernel/net/rtl8139.h"
#include "kernel/net/dhcp.h"
#include "kernel/sound.h"
#include "startup.h"
#include "drivers/io.h"
#include "drivers/mouse/mouse.h"
#include "drivers/keyboard/keyboard.h"

LangID   g_lang  = LANG_EN;
ThemeID  g_theme = THEME_LUNA;
int      g_wallpaper = 0;
bool     g_dev_mode  = false;
char     g_current_user[32] = "";  /* Login ekranında seçilen kullanıcı adı */
uint32_t* backbuffer = (uint32_t*)0x04000000UL /* 1024×768×4=3MB — @0x4000000 */; /* 64MB — BSS/NIC sonrası */
VFS      g_vfs;
NetState g_net;
RTL8139  g_rtl;
E1000    g_e1000;
TCPConn  g_tcp_conn;
bool     g_net_ready = false;
MsgBox   g_msgbox;
uint16_t g_pit_last  = 0;
uint32_t g_pit_ticks = 0;
static inline uint16_t pit_read_count(){
    __asm__ volatile("outb %0,%1"::"a"((uint8_t)0x00),"Nd"((uint16_t)0x43));
    uint8_t lo,hi;
    __asm__ volatile("inb %1,%0":"=a"(lo):"Nd"((uint16_t)0x40));
    __asm__ volatile("inb %1,%0":"=a"(hi):"Nd"((uint16_t)0x40));
    return (uint16_t)((hi<<8)|lo);
}
Compositor* g_comp_ptr = nullptr;

extern uint32_t* vram;
extern uint32_t  screen_pitch;
extern uint32_t  mem_upper_kb;

#include "apps/notepad.h"
#include "apps/calculator.h"
#include "apps/filemgr.h"
#include "apps/settings.h"
#include "apps/terminal.h"
#include "apps/paint.h"
#include "apps/sysinfo.h"
#include "apps/clock_app.h"
#include "apps/devtools.h"
#include "apps/browser.h"
#include "apps/calendar.h"
#include "apps/taskman.h"
#include "apps/games/snake.h"
#include "apps/games/tetris.h"
#include "apps/games/pong.h"
#include "apps/games/minesweeper.h"
#include "apps/games/games_folder.h"
#include "apps/luigi.h"
#include "drivers/ata.h"
#include "kernel/vfs_persist.h"
#include "apps/screenshot.h"
#include "apps/hexeditor.h"
#include "apps/music.h"
#include "apps/games/breakout.h"
#include "bsod.h"

/* ═══════════════════════════════════════════
   WAIT / SCALED TEXT
   ═══════════════════════════════════════════ */
static void BusyWait(unsigned int n){
    for(volatile unsigned int i=0;i<n;i++) __asm__ volatile("nop");
}
static void DrawScaledText(const char* msg,int bx,int by,int scale,uint32_t color){
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
static void SwapAll(){
    /*
     * Backbuffer stride: her zaman SW*4 (compile-time sabit)
     * VRAM dst stride : screen_pitch (GRUB'dan runtime)
     * Her satır: min(SW, screen_w)*4 byte kopyalanır
     *
     * Bu sayede:
     *  - GRUB 800x600 verdi, SW=1024 → sadece 800px kopyalanır (satırlar doğru)
     *  - GRUB 1024x768 verdi, SW=1024 → tam eşleşme
     *  - Her iki durumda da pitch kayması olmaz
     *
     * Not: Önceden movntdq (non-temporal store) kullanılıyordu. Bu, siyah
     * ekran bugunun (asıl sebep: yanlış hard-coded VRAM adresi) teşhisi
     * sırasında sade 4-byte'lık mov ile geçici olarak değiştirilmişti — bu
     * FPS'i belirgin şekilde düşürdü (SIMD'siz, satır başına ~10x daha
     * fazla instruction). VRAM adresi artık PCI'dan doğru okunduğu için
     * SIMD'e geri dönüyoruz, ama non-temporal (movntdq) yerine normal
     * movdqu store kullanıyoruz: aynı 16-byte/instr hızı, ama WC bellek
     * tipi garantisi gerektirmediği için her ortamda güvenli.
     */
    uint8_t* src = (uint8_t*)backbuffer;
    uint8_t* dst = (uint8_t*)vram;
    const uint32_t src_stride  = SW * 4;          /* backbuffer satır genişliği */
    const uint32_t copy_w      = screen_w < SW ? screen_w : SW;
    const uint32_t copy_bytes  = copy_w * 4;
    const uint32_t dst_stride  = screen_pitch;     /* GRUB/BGA gerçek stride */
    const uint32_t rows        = screen_h < SH ? screen_h : SH;
    for(uint32_t y = 0; y < rows; y++){
        uint8_t* s = src + y * src_stride;
        uint8_t* d = dst + y * dst_stride;
        uint32_t chunks = copy_bytes / 16;
        uint32_t remain = copy_bytes % 16;
        if(chunks){
            __asm__ volatile(
                "1:movdqu (%0),%%xmm0\nmovdqu %%xmm0,(%1)\n"
                "add $16,%0\nadd $16,%1\ndec %2\njnz 1b\n"
                :"+r"(s),"+r"(d),"+r"(chunks)::"xmm0","memory");
        }
        for(uint32_t i = 0; i < remain; i++) d[i] = s[i];
    }
}

/* ═══════════════════════════════════════════
   SHUTDOWN / RESTART SCREENS
   ═══════════════════════════════════════════ */
static void PowerScreen(const char* action_text,uint32_t bar_col){
    for(uint32_t i=0;i<SW*SH;i++) backbuffer[i]=0;
    SwapAll(); BusyWait(6000000u);
    for(int row=0;row<(int)SH;row++)
        DrawRect(0,row,SW,1,LerpColor(0x080810,0x141428,row,SH));
    DrawRect(SW/2-100,SH/2-18,200,1,0x304858);
    DrawScaledText("LightOS",SW/2-7*12,SH/2-52,3,0x80B8E0);
    DrawScaledText(action_text,SW/2-(int)(9*9),SH/2+2,2,0xCCDDEE);
    int bx=SW/2-100,by=SH/2+36,bw=200,bh=5;
    DrawRect(bx-1,by-1,bw+2,bh+2,0x304858);
    DrawRect(bx,by,bw,bh,0x0C1820);
    for(int step=1;step<=5;step++){
        int filled=bw*step/5;
        for(int px2=0;px2<filled;px2++)
            DrawRect(bx+px2,by,1,bh,LerpColor(bar_col,BrightColor(bar_col,150),px2,bw));
        SwapAll(); BusyWait(40000000u);
    }
    for(uint32_t i=0;i<SW*SH;i++) backbuffer[i]=0;
    SwapAll(); BusyWait(15000000u);
}
static void DoShutdown(){
    acpi_ensure_init();
    PlayShutdown(); /* Kapanış melodisi */
    PowerScreen("Shutting down...",0x2060A0);
    serial_puts(COM1, g_disk.present ? "[LightOS] DoShutdown: disk present, calling vfs_save()\n"
                                      : "[LightOS] DoShutdown: g_disk.present is FALSE, skipping vfs_save()\n");
    if(g_disk.present){
        bool ok = vfs_save();
        serial_puts(COM1, ok ? "[LightOS] vfs_save() = true\n" : "[LightOS] vfs_save() = FALSE (write failed!)\n");
    }
    acpi_shutdown();
    hal_outw(0x604,0x2000);
    hal_outw(0xB004,0x2000);
    hal_cli(); hal_hlt();
}
static void DoRestart(){
    acpi_ensure_init();
    PlayShutdown();
    PowerScreen("Restarting...",0x208040);
    serial_puts(COM1, g_disk.present ? "[LightOS] DoRestart: disk present, calling vfs_save()\n"
                                      : "[LightOS] DoRestart: g_disk.present is FALSE, skipping vfs_save()\n");
    if(g_disk.present){
        bool ok = vfs_save();
        serial_puts(COM1, ok ? "[LightOS] vfs_save() = true\n" : "[LightOS] vfs_save() = FALSE (write failed!)\n");
    }
    acpi_restart();
    while(1) hal_hlt();
}

/* ═══════════════════════════════════════════════════
   DESKTOP ICONS — otomatik grid, 1024x768 için
   ═══════════════════════════════════════════════════
   ICON_W=56, ICON_H=72 (ikon+label), GAP=16
   Sütun sayısı: soldan GRID_COLS sütun
   Satır sayısı: otomatik
   ═══════════════════════════════════════════════════ */
static const int ICON_W   = 56;   /* ikon genişliği (piksel çerçeve dahil) */
static const int ICON_H   = 72;   /* ikon yüksekliği (ikon+label) */
static const int ICON_GAP = 14;   /* ikonlar arası boşluk */
static const int GRID_X   = 16;   /* sol başlangıç */
static const int GRID_Y   = 16;   /* üst başlangıç */
static const int GRID_COLS = 3;   /* sütun sayısı */

struct DeskIcon{StrID lid; int app_id;};
static DeskIcon desk_icons[]={
    {S_NOTEPAD,    0},
    {S_FILES,      1},
    {S_CALCULATOR, 2},
    {S_SETTINGS,   3},
    {S_TERMINAL,   4},
    {S_PAINT,      5},
    {S_SYSINFO,    6},
    {S_CLOCK,      7},
    {S_BROWSER,    8},
    {S_CALENDAR,   9},
    {S_TASKMAN,   10},
    {S_XAEFGAMES, 11},
    {S_LUIGI,     12},
    {S_HEXED,     13},
    {S_MUSIC,     14},
};
static const int ICON_N=15;

/* İkon grid koordinatı hesapla */
static void icon_pos(int idx, int& ox, int& oy){
    int col=idx%GRID_COLS, row=idx/GRID_COLS;
    ox=GRID_X+col*(ICON_W+ICON_GAP);
    oy=GRID_Y+row*(ICON_H+ICON_GAP);
}

static void DrawRoundRect(int rx,int ry,int rw,int rh,int radius,uint32_t col){
    DrawRect(rx+radius,ry,rw-radius*2,rh,col);
    DrawRect(rx,ry+radius,rw,rh-radius*2,col);
    for(int dy=0;dy<radius;dy++){
        int dx=(int)(radius-1);
        while(dx*dx+(dy-radius+1)*(dy-radius+1)>radius*radius&&dx>0) dx--;
        int px=rw-radius-dx;
        int py=dy;
        DrawRect(rx+radius-dx-1,ry+py,dx+1,1,col);
        DrawRect(rx+px,ry+py,dx+1,1,col);
        DrawRect(rx+radius-dx-1,ry+rh-1-py,dx+1,1,col);
        DrawRect(rx+px,ry+rh-1-py,dx+1,1,col);
    }
}
static void DrawRoundRectBorder(int rx,int ry,int rw,int rh,int radius,uint32_t col){
    /* Üst/alt yatay */
    DrawRect(rx+radius,ry,rw-radius*2,1,col);
    DrawRect(rx+radius,ry+rh-1,rw-radius*2,1,col);
    /* Sol/sağ dikey */
    DrawRect(rx,ry+radius,1,rh-radius*2,col);
    DrawRect(rx+rw-1,ry+radius,1,rh-radius*2,col);
    /* Köşe yayları (basit 4 piksel) */
    for(int a=0;a<radius;a++){
        int b=(int)(radius-1);
        while(b*b+(a-radius+1)*(a-radius+1)>radius*radius&&b>0) b--;
        PutPixel(rx+radius-b-1,ry+a,col);
        PutPixel(rx+rw-radius+b,ry+a,col);
        PutPixel(rx+radius-b-1,ry+rh-1-a,col);
        PutPixel(rx+rw-radius+b,ry+rh-1-a,col);
    }
}

static void DrawDeskIcon(int ix,int iy,int app_id){
    uint32_t cols[]={0x2C6FBF,0xD98A0F,0x1F9955,0x2A3542,0x0C0C0C,0xC23D6B,0x0C0C18,0x2C3E50,0x2A5FB0,0xB8305F,0x1F8F45,0x4638C2,0x0F9A80,0x7A3FA8,0xD94F4F};
    uint32_t bg=cols[app_id<15?app_id:0];

    /* Terminal(4) ve System Info(6) orijinal tasarımlarını koruyor,
     * diğer tüm ikonlar modernize edildi: kare köşeli, tam dolgun,
     * koyu-doygun zemin renkleri üzerinde temiz/sade siluetler. */
    if(app_id!=4 && app_id!=6){
        DrawRect(ix,iy,40,44,DimColor(bg,92));
        DrawRect(ix,iy,40,20,BrightColor(bg,112));
        DrawRectBorder(ix,iy,40,44,DimColor(bg,50));
        DrawRect(ix+1,iy+1,38,1,LerpColor(bg,0xFFFFFF,35,100)); /* üst ince parlaklık */
    }

    /* Mini ikon sembol — kutuyu (40x44) olabildiğince dolduracak boyutta */
    switch(app_id){
        case 0: /* Notepad — kağıt + satırlar, tam yükseklikte */
            DrawRect(ix+5,iy+3,30,38,0xF0EDE4);
            DrawRectBorder(ix+5,iy+3,30,38,0xA8A294);
            DrawRect(ix+5,iy+3,7,7,0xD8D2C0); /* kıvrık köşe hissi */
            for(int i=0;i<6;i++) DrawRect(ix+10,iy+14+i*4,20,2,0x7078B0);
            break;
        case 1: /* Files — klasör, tam genişlik */
            DrawRect(ix+3,iy+16,34,23,0xE8B448);
            DrawRect(ix+3,iy+10,17,8,0xE8B448);
            DrawRectBorder(ix+3,iy+16,34,23,0xA87818);
            DrawRect(ix+3,iy+16,34,4,0xFFD880);
            break;
        case 2: /* Calc — LCD ekran + tuşlar, tam kutu */
            DrawRect(ix+3,iy+3,34,12,0x081408);
            DrawString(ix+27,iy+7,"0",0x40E070);
            for(int r2=0;r2<3;r2++) for(int c=0;c<3;c++)
                DrawRect(ix+4+c*11,iy+18+r2*8,9,6,0xD8D8D8);
            break;
        case 3: /* Settings — dişli, büyük ve net */
            DrawRect(ix+17,iy+3,6,8,0xB8C0C8);
            DrawRect(ix+17,iy+33,6,8,0xB8C0C8);
            DrawRect(ix+3,iy+17,8,6,0xB8C0C8);
            DrawRect(ix+29,iy+17,8,6,0xB8C0C8);
            DrawRect(ix+8,iy+8,7,7,0xB8C0C8);
            DrawRect(ix+25,iy+8,7,7,0xB8C0C8);
            DrawRect(ix+8,iy+29,7,7,0xB8C0C8);
            DrawRect(ix+25,iy+29,7,7,0xB8C0C8);
            DrawRect(ix+9,iy+9,22,22,0xD0D6DC);
            DrawRect(ix+16,iy+16,8,8,0x2A3542);
            break;
        case 5: /* Paint — palet, tam kutu */
            DrawRect(ix+3,iy+3,34,26,0xF5F3EC);
            DrawRectBorder(ix+3,iy+3,34,26,0xB8B4A4);
            DrawRect(ix+7,iy+7,10,10,0xE84040);
            DrawRect(ix+21,iy+7,10,10,0x30A840);
            DrawRect(ix+7,iy+17,10,10,0x3878E0);
            DrawRect(ix+21,iy+17,10,10,0xE8C020);
            DrawRect(ix+12,iy+31,16,10,0x7868C8);
            break;
        case 8: /* Browser — arama çubuklu pencere, tam kutu */
            DrawRect(ix+3,iy+4,34,36,0xFFFFFF);
            DrawRect(ix+3,iy+4,34,11,0xD8E6F8);
            DrawRect(ix+6,iy+7,22,5,0x5090D8);
            DrawRect(ix+6,iy+20,28,3,0xC8D8E8);
            DrawRect(ix+6,iy+27,28,3,0xC8D8E8);
            DrawRect(ix+6,iy+34,20,3,0xC8D8E8);
            break;
        case 11: /* XaefGAMES — oyun kolu, tam genişlik */
            DrawRect(ix+3,iy+13,34,22,0x2848A8);
            DrawRect(ix+9,iy+21,8,3,0xA8C8FF);
            DrawRect(ix+12,iy+18,3,8,0xA8C8FF);
            DrawRect(ix+24,iy+20,5,5,0xE85050);
            DrawRect(ix+31,iy+20,5,5,0x50D888);
            break;
        case 13: /* Hex Editor — kod satırları, tam kutu */
            DrawRect(ix+3,iy+3,34,38,0x120826);
            DrawRectBorder(ix+3,iy+3,34,38,0x6838A8);
            DrawString(ix+6,iy+8,"0A1B",0x9868E0);
            DrawString(ix+6,iy+18,"2C3D",0x7048B8);
            DrawString(ix+6,iy+28,"EFFF",0x9868E0);
            break;
        case 14: /* Music Player — nota, tam kutu */
            DrawRect(ix+3,iy+3,34,38,0x241C04);
            DrawRectBorder(ix+3,iy+3,34,38,0xD8B838);
            DrawRect(ix+15,iy+8,3,18,0xF0D050);
            DrawRect(ix+25,iy+6,3,18,0xF0D050);
            DrawRect(ix+15,iy+7,13,3,0xF0D050);
            DrawRect(ix+11,iy+24,7,7,0xF0D050);
            DrawRect(ix+21,iy+22,7,7,0xF0D050);
            break;
        case 12: /* Luigi AI — dost robot yüzü, tam kutu */
            DrawRect(ix+4,iy+8,32,26,0x082418);
            DrawRectBorder(ix+4,iy+8,32,26,0x40D8A0);
            DrawRect(ix+11,iy+17,6,7,0x40D8A0);
            DrawRect(ix+23,iy+17,6,7,0x40D8A0);
            DrawRect(ix+14,iy+29,12,3,0x40D8A0);
            DrawRect(ix+17,iy+2,6,7,0x40D8A0);
            break;
        case 9: /* Calendar — modern takvim, tam kutu */
            DrawRect(ix+3,iy+6,34,34,0xF5F3EC);
            DrawRect(ix+3,iy+6,34,11,0xC23D6B);
            DrawRect(ix+9,iy+2,4,8,0x902A4E);
            DrawRect(ix+27,iy+2,4,8,0x902A4E);
            for(int rr=0;rr<3;rr++) for(int cc=0;cc<5;cc++)
                DrawRect(ix+6+cc*5,iy+21+rr*6,4,4,rr==0&&cc==2?0xC23D6B:0xD8CCD0);
            break;
        case 10: /* Task Manager — performans çubukları, tam kutu */
            DrawRect(ix+3,iy+3,34,38,0xE8F5EC);
            DrawRectBorder(ix+3,iy+3,34,38,0x60B878);
            DrawRect(ix+7,iy+29,5,8,0x1F9955);
            DrawRect(ix+15,iy+21,5,16,0x1F9955);
            DrawRect(ix+23,iy+13,5,24,0x188040);
            DrawRect(ix+31,iy+25,5,12,0x1F9955);
            break;
        case 4: /* Terminal — değişmedi */
            DrawRect(ix+2,iy+2,36,40,0x0C0C0C);
            DrawString(ix+4,iy+8,">_",0x00FF44);
            DrawString(ix+4,iy+20,"ls",0x44AAFF);
            break;
        case 6: /* Sys info — değişmedi */
            DrawGradientV(ix+2,iy+2,36,40,0x081830,0x102040);
            DrawString(ix+4,iy+8,"SYS",0x80C0FF);
            DrawRect(ix+4,iy+20,32,4,0x2288AA);
            DrawRect(ix+4,iy+28,20,4,0x44CC44);
            DrawRect(ix+4,iy+34,10,4,0xFF8844);
            break;
        case 7: /* Clock — modern minimal saat yüzü, tam kutu */
            DrawRect(ix+3,iy+3,34,38,0xF0EDE6);
            DrawRectBorder(ix+3,iy+3,34,38,0xA8A294);
            DrawRect(ix+19,iy+6,2,4,0x807A68);
            DrawRect(ix+19,iy+34,2,4,0x807A68);
            DrawRect(ix+6,iy+21,4,2,0x807A68);
            DrawRect(ix+34,iy+21,4,2,0x807A68);
            DrawRect(ix+19,iy+12,2,10,0x282828);
            DrawRect(ix+20,iy+21,9,2,0xD84040);
            DrawRect(ix+19,iy+20,2,2,0x282828);
            break;
    }
}

static void DrawDesktopIcons(Mouse& m){
    for(int i=0;i<ICON_N;i++){
        DeskIcon& ic=desk_icons[i];
        int ix2,iy2; icon_pos(i,ix2,iy2);
        bool h=(m.x>=ix2-2&&m.x<=ix2+ICON_W+2&&m.y>=iy2-2&&m.y<=iy2+ICON_H+2);
        if(h){
            DrawRect(ix2-3,iy2-3,ICON_W+6,ICON_H+6,LerpColor(TP().win_body,WP_FrameFoc(),15,100));
            DrawRectBorder(ix2-3,iy2-3,ICON_W+6,ICON_H+6,WP_FrameFoc());
        }
        /* İkon kutusunu ortala */
        int icon_box_x=ix2+(ICON_W-40)/2;
        DrawDeskIcon(icon_box_x,iy2,ic.app_id);
        const char* lbl=LS(ic.lid);
        int tw=StringWidth(lbl),lx=ix2+ICON_W/2-tw/2;
        /* Etiket rengi: sabit siyah kutu yerine, altındaki zeminin (duvar
         * kağıdı) gerçek rengine göre otomatik siyah/beyaz seçilir, böylece
         * her duvar kağıdında okunaklı kalır ve göze batan opak siyah şerit
         * kalkar. Zemin rengini etiketin ortasından örnekliyoruz. */
        int sample_x=lx+tw/2, sample_y=iy2+50;
        uint32_t bgSample = (sample_x>=0&&sample_x<(int)SW&&sample_y>=0&&sample_y<(int)SH)
            ? backbuffer[sample_y*SW+sample_x] : 0x2050A0;
        DrawStringAdaptive(lx, iy2+46, lbl, bgSample);
    }

    /* Dev Mode masaüstü ikonları — sağ tarafta, 2 sütun.
     * Önceki tasarımda aynı etiket hem ikon içinde hem altında iki kez
     * yazılıyordu (karışık görünüm) ve sütunlar arası boşluk (58px'te
     * 40px ikon) bazı etiketlerin bitişik sütuna taşmasına yetecek kadar
     * dardı. Artık etiket sadece ikonun ALTINDA bir kez yazılıyor, kutu
     * genişliği ve sütun aralığı etiketlere göre büyütüldü, hit-alanı
     * (aşağıdaki tıklama handling ile) buradaki görsel alanla birebir
     * aynı sabitlerden hesaplanıyor. */
    if(g_dev_mode){
        const char* dev_labels[]={"MemView","Port I/O","VFS","CPU Regs",
                                   "IRQ Mon","Stack","KrnLog","BSOD Test",
                                   "RTC","Panic"};
        uint32_t dev_cols[]={0xFF4444,0xFF8800,0x44FFFF,0xFFFF44,0xFF44FF,
                              0x44FF88,0x8899FF,0xFF0000,0xFFCC00,0xFF2222};
        const int DEV_ICON_W=44, DEV_ICON_H=40, DEV_COL_GAP=72, DEV_ROW_GAP=78;
        for(int i=0;i<10;i++){
            int col=i%2, row=i/2;
            int dix=SW-8-DEV_COL_GAP*2+col*DEV_COL_GAP;
            int diy=8+row*DEV_ROW_GAP;
            bool dh=(m.x>=dix-2&&m.x<=dix+DEV_ICON_W+2&&m.y>=diy-2&&m.y<=diy+DEV_ROW_GAP-8);
            if(dh){DrawRect(dix-4,diy-4,DEV_ICON_W+8,DEV_ROW_GAP-4,0x1A0800);
                   DrawRectBorder(dix-4,diy-4,DEV_ICON_W+8,DEV_ROW_GAP-4,0xFF8800);}
            /* Dev ikon: koyu turuncu kare + kısa "DEV" etiketi + renkli
             * ayırıcı çizgi — asıl (uzun) isim YALNIZCA ikonun altında,
             * bir kez yazılıyor (aşağıda). Önceden burada dev_labels[i]
             * bir kez daha yazılıyordu; ikon 40px genişlikte olduğu için
             * "Port I/O", "BSOD Test" gibi uzun isimler kutunun dışına
             * taşıp bitişik sütunla karışıyordu ("karışık" görünüm). */
            DrawGradientV(dix,diy,DEV_ICON_W,DEV_ICON_H,0x1A0800,0x0A0400);
            DrawRectBorder(dix,diy,DEV_ICON_W,DEV_ICON_H,0xFF8800);
            DrawStringCentered(dix+DEV_ICON_W/2,diy+8,"DEV",0xFF8800);
            DrawRect(dix+4,diy+24,DEV_ICON_W-8,3,dev_cols[i]);
            /* Etiket — SADECE burada, ikon kutusunun altında.
             * Altındaki gerçek zemin rengine göre kontrast metin (aynı
             * mantık masaüstü ikon etiketlerinde de kullanılıyor). */
            int tw2=StringWidth(dev_labels[i]);
            int lx2=dix+DEV_ICON_W/2-tw2/2, ly2=diy+DEV_ICON_H+4;
            uint32_t bgS=(lx2>=0&&lx2<(int)SW&&ly2>=0&&ly2<(int)SH)
                ? backbuffer[ly2*SW+lx2] : 0x101820;
            DrawStringAdaptive(lx2, ly2, dev_labels[i], bgS);
        }
    }
}

/* ═══════════════════════════════════════════
   TASKBAR + CLOCK
   ═══════════════════════════════════════════ */
static bool qmenu_open=false,ctx_open=false;

/* ── Dock/taskbar geometrisi — TEK KAYNAK ──
 * DrawTaskbar() (çizim) ve TaskbarHit()/tıklama kodu (satır ~910+) eskiden
 * birbirinden bağımsız, farklı sabitler kullanıyordu (dock_w 600 vs 840,
 * dock_x SW/2-300 vs SW/2-420 gibi) — bu yüzden QuickM/Start butonu görsel
 * olarak bir yerde duruyor ama tıklama alanı başka bir yerdeydi ("Start
 * menu çalışmıyor" bugu). Artık hem çizim hem hit-test bu tek fonksiyonu
 * çağırıyor, iki taraf asla birbirinden sapamaz.
 *
 * Klasik Windows tarzı: tam genişlik, ekranın alt kenarına tam yapışık
 * (macOS-dock tarzı ortada yüzen kapsülün yerini aldı). */
struct DockGeom { int x,y,w,h; };
static inline DockGeom GetDockGeom(){
    DockGeom d;
    d.h = 40;
    d.y = SH - d.h;
    d.x = 0;
    d.w = SW;
    return d;
}
static int  ctx_x=0,ctx_y=0;



static void DrawTaskbar(Compositor& comp,Mouse& m){
    /*
     * Klasik Windows tarzı taskbar: tam genişlik, ekranın alt kenarına
     * tam yapışık, köşeli/az yuvarlatılmış dikdörtgen şerit. Öncesinde
     * macOS-dock tarzı ortada yüzen yuvarlak kapsüldü.
     */
    DockGeom dg = GetDockGeom();
    int dock_h = dg.h;
    int dock_y = dg.y;
    int dock_x = dg.x;
    int dock_w = dg.w;

    uint32_t bar_top = TP().taskbar_top;
    uint32_t bar_bot = TP().taskbar_bot;

    /* ── Arka plan: düz dikey gradient, tam genişlik ── */
    DrawGradientV(dock_x, dock_y, dock_w, dock_h, BrightColor(bar_top,108), bar_bot);
    /* Üst kenar çizgisi — ekranı taskbar'dan ayıran ince parlak hat */
    DrawRect(dock_x, dock_y, dock_w, 1, BrightColor(bar_top,150));
    DrawRect(dock_x, dock_y+1, dock_w, 1, LerpColor(bar_top,0x000000,20,100));

    /* ── Start butonu (sol) ── */
    int qx=dock_x+2, qy=dock_y+2, qw=88, qh=dock_h-4, qr=3;
    bool shov=(m.x>=qx&&m.x<=qx+qw&&m.y>=dock_y&&m.y<=dock_y+dock_h);
    uint32_t qcol=qmenu_open
        ?LerpColor(WP_TitleTop(true),0x000000,15,100)
        :(shov?BrightColor(WP_TitleTop(true),112):WP_TitleTop(true));
    DrawRoundRect(qx,qy,qw,qh,qr,qcol);
    if(!qmenu_open) DrawRect(qx+1,qy+1,qw-2,1,BrightColor(qcol,140));
    DrawRoundRectBorder(qx,qy,qw,qh,qr,LerpColor(qcol,0x000000,30,100));
    DrawStringCentered(qx+qw/2,qy+qh/2-4,LS(S_MENU_TITLE),WP_TitleText(true));

    /* Separator */
    DrawRect(dock_x+qw+6,dock_y+5,1,dock_h-10,LerpColor(bar_top,0x000000,30,100));
    DrawRect(dock_x+qw+7,dock_y+5,1,dock_h-10,LerpColor(bar_top,0xFFFFFF,15,100));

    /* ── Pencere butonları (sol-orta, sabit genişlik dikdörtgenler) ── */
    int bx=dock_x+qw+14;
    int right_reserved = 210; /* dil butonu + saat + logo için ayrılan sağ alan */
    for(int i=0;i<comp.count;i++){
        Window* ww=comp.Get(i); if(!ww->visible) continue;
        bool act=(i==comp.count-1)&&!ww->minimized;
        int bw2=140, bh2=dock_h-6, br=2;
        if(bx+bw2>dock_x+dock_w-right_reserved) break;
        bool bhov=(m.x>=bx&&m.x<=bx+bw2&&m.y>=dock_y&&m.y<=dock_y+dock_h);
        uint32_t bcol=act
            ?LerpColor(WP_TitleTop(true),0x000000,10,100)
            :(bhov?BrightColor(bar_top,118):LerpColor(bar_top,0x000000,6,100));
        DrawRoundRect(bx,dock_y+3,bw2,bh2,br,bcol);
        DrawRoundRectBorder(bx,dock_y+3,bw2,bh2,br,LerpColor(bcol,0x000000,25,100));
        if(act){
            /* Aktif göstergesi — üstten ince renkli çizgi */
            DrawRect(bx+2,dock_y+3,bw2-4,2,WP_FrameFoc());
        }
        char tb[17]; int j=0;
        while(ww->title[j]&&j<15){tb[j]=ww->title[j];j++;}
        if(ww->title[j]){tb[j]='~';j++;} tb[j]=0;
        DrawString(bx+8,dock_y+dock_h/2-4,tb,act?WP_TitleText(true):LerpColor(WP_TitleText(true),bar_top,35,100));
        bx+=146;
    }

    /* ── Sağ separator ── */
    int tray_x=dock_x+dock_w-right_reserved+8;
    DrawRect(tray_x-6,dock_y+5,1,dock_h-10,LerpColor(bar_top,0x000000,30,100));
    DrawRect(tray_x-5,dock_y+5,1,dock_h-10,LerpColor(bar_top,0xFFFFFF,15,100));

    /* ── Sistem tepsisi (sağ) ── */
    /* Dil butonu */
    int lx=tray_x, ly=dock_y+3, lw=40, lh=dock_h-6, lr=2;
    bool lhov=(m.x>=lx&&m.x<=lx+lw&&m.y>=dock_y&&m.y<=dock_y+dock_h);
    uint32_t lcol=lhov?BrightColor(bar_top,118):LerpColor(bar_top,0x000000,6,100);
    DrawRoundRect(lx,ly,lw,lh,lr,lcol);
    DrawRoundRectBorder(lx,ly,lw,lh,lr,LerpColor(lcol,0x000000,25,100));
    DrawStringCentered(lx+lw/2,ly+lh/2-4,LS(S_LANGUAGE),0xFFCC44);

    /* Ayırıcı */
    DrawRect(lx+lw+6,dock_y+5,1,dock_h-10,LerpColor(bar_top,0x000000,20,100));

    /* Saat + tarih bloğu (klasik Windows sistem saati kutusu hissi) */
    int clock_x = lx+lw+14;
    int clock_w = right_reserved-lw-30;
    RtcTime t=rtc_get();
    char h2c[3],m2c[3]; rtc_2d(t.hour,h2c); rtc_2d(t.min,m2c);
    char ts[8]; ts[0]=h2c[0];ts[1]=h2c[1];ts[2]=':';ts[3]=m2c[0];ts[4]=m2c[1];ts[5]=0;
    DrawStringCentered(clock_x+clock_w/2,dock_y+dock_h/2-8,ts,WP_TitleText(true));
    DrawStringCentered(clock_x+clock_w/2,dock_y+dock_h/2+2,"LightOS",LerpColor(WP_TitleText(true),bar_top,35,100));
}

/* ═══════════════════════════════════════════
   QUICKM MENU
   ═══════════════════════════════════════════ */
/* NOT: Burada eskiden DrawDevBar() adında, masaüstünün sağ kenarında
 * sabit duran ikinci bir "DEV TOOLS" paneli vardı. Bu panelin HİÇBİR
 * tıklama karşılığı yoktu (tamamen dekoratifti — hangi butona basılırsa
 * basılsın hiçbir şey olmuyordu) ve üstelik DrawDesktopIcons() içindeki
 * gerçek, tıklanabilir Dev Mode masaüstü ikonlarıyla AYNI ekran bölgesini
 * (sağ kenar) kaplayarak üst üste biniyordu — "sidebar'daki butonlara
 * tıklanmıyor / karışık" şikâyetinin kaynağı tam olarak buydu. Dev Tools
 * erişimi zaten iki çalışan yoldan sağlanıyor: masaüstü ikonları ve
 * Start Menu'nün sağ paneli (Dev Mode'dayken). Bu yüzden işlevsiz ve
 * çakışan üçüncü kopya kaldırıldı. */

static void DrawQuickMenu(Mouse& m){
    if(!qmenu_open) return;

    /* ── Boyutlar: içerik miktarına göre DİNAMİK hesaplanır ──
     * Normal modda tek sütun, dinamik yükseklik — taşma yapısal olarak
     * imkansız. Dev Mode'da ekstra 11 satır (10 dev tool + başlık) tek
     * sütuna sığdırılınca (eski tasarım) toplam yükseklik ekranı aşıyor,
     * kırpma devreye girince kırpılan panel sınırlarının dışına çizim/
     * tıklama düşüyordu ("menü taşıyor", "bazı butonlara tıklanmıyor").
     * Çözüm: Dev Mode'da ikinci bir sütun açılır (sağda, sadece dev
     * tools + Restart/Shutdown) — hiçbir satır ekranın kalan yüksekliğini
     * aşmaz. Aynı zamanda Dev Mode'u görsel olarak da ayırt etmek için
     * köşeler kare (r=0), normal modda yuvarlak (r=18) kalır. */
    const int ROW_H     = 30;   /* uygulama satırı yüksekliği */
    const int HEADER_H  = 56;   /* logo+başlık şeridi */
    const int FOOTER_PAD= 10;   /* alt boşluk */
    const int SEP_H     = 9;    /* Restart/Shutdown öncesi ayırıcı boşluk */

    struct MI{StrID id;int app_id;uint32_t dot;};
    static const MI items[]={
        {S_NOTEPAD,   0,0x5B9CFF},{S_CALCULATOR,2,0x4ADE80},
        {S_FILES,     1,0xFBBF24},{S_SETTINGS,  3,0xD08A4C},
        {S_TERMINAL,  4,0x34D399},{S_PAINT,     5,0xFB6BA8},
        {S_SYSINFO,   6,0x60A5FA},{S_CLOCK,     7,0xFACC15},
        {S_BROWSER,   8,0x38BDF8},
        {S_CALENDAR,  9,0xC2437A},
        {S_TASKMAN,  10,0x2FA84F},
        {S_LUIGI,    12,0x5EEAD4},
        {S_HEXED,   13,0x9B7BFF},
        {S_MUSIC,   14,0xFDE047},
    };
    const int N_ITEMS = 14;
    static const char* dnames[]={"Mem Viewer","Port I/O","VFS Inspector",
                           "CPU Regs","IRQ Monitor","Stack View",
                           "Kernel Log","BSOD Tester","RTC Inspector",
                           "Panic Sim"};
    static const uint32_t ddots[]={0xFF4444,0xFF8800,0x44FFFF,0xFFFF44,0xFF44FF,
                       0x44FF88,0x88AAFF,0xFF0000,0xFFCC00,0xFF2222};
    const int N_DEV = 10;

    int mw = 264;
    /* Sol sütun (uygulamalar) her zaman aynı: sadece N_ITEMS satır.
     * Dev Mode'daki ekstra içerik artık AYRI bir sağ panelde. */
    int mh = HEADER_H + N_ITEMS*ROW_H + SEP_H + 2*ROW_H + FOOTER_PAD;

    DockGeom dg = GetDockGeom();
    int mx2 = dg.x;
    int my2 = dg.y - mh - 10;      /* dock'un 10px üstünde biter */
    if(my2 < 8) my2 = 8;           /* ekran üstünden taşmasın */
    /* Güvenlik ağı: yine de sığmıyorsa kırp (artık pratikte tetiklenmez
     * çünkü sabit N_ITEMS satırlık içerik her ekran boyutunda sığar). */
    int max_h = SH - my2 - 8;
    if(mh > max_h) mh = max_h;

    int r = g_dev_mode ? 0 : 18; /* Dev Mode: kare köşe; normal: yuvarlak */

    DrawShadow(mx2,my2,mw,mh);
    /* Cam benzeri koyu-şeffaf panel, dock ile aynı dil */
    uint32_t panel_top = LerpColor(TP().taskbar_top, 0x000000, 10, 100);
    uint32_t panel_bot = LerpColor(TP().taskbar_bot, 0x000000, 20, 100);
    DrawRoundRect(mx2,my2,mw,mh,r,panel_bot);
    for(int row=0;row<mh;row++){
        uint32_t c2=LerpColor(panel_top,panel_bot,row,mh);
        int rowr = (row<r)?(r-row):((row>mh-r)?(row-(mh-r)):0);
        int inset = 0;
        if(row<r){
            /* üst köşeleri yuvarlatmak için basit yaklaşım: DrawRoundRect
             * zaten üst/alt köşeleri çizdi, burada sadece dolgu rengini
             * satır satır güncelliyoruz (köşe maskesine dokunmuyoruz) */
            inset = 0;
        }
        (void)rowr;
        for(int px=mx2+inset;px<mx2+mw-inset;px++){
            if(px<0||px>=(int)SW) continue;
            int yy=my2+row; if(yy<0||yy>=(int)SH) continue;
            /* Sadece iç alanı boyuyoruz; köşe pikselleri DrawRoundRect'in
             * bıraktığı yuvarlatılmış maskeyle örtüşsün diye satırın en
             * dış 0 pikseli hariç tutulmaz — basit dikey gradient yeterli */
            backbuffer[yy*SW+px]=c2;
        }
    }
    DrawRoundRectBorder(mx2,my2,mw,mh,r,LerpColor(panel_top,0xFFFFFF,22,100));
    if(TP().ice_glass) DrawRect(mx2+r,my2+1,mw-2*r,1,LerpColor(panel_top,0xFFFFFF,35,100));

    /* ── Header ── */
    DrawStringCentered(mx2+mw/2, my2+14, "LightOS", WP_TitleText(true));
    DrawStringCentered(mx2+mw/2, my2+27,
        g_dev_mode?"Developer Mode":"2 Pro",
        g_dev_mode?0xFF8800:LerpColor(WP_TitleText(true),0x000000,35,100));
    DrawRect(mx2+16,my2+HEADER_H-1,mw-32,1,LerpColor(panel_top,0xFFFFFF,15,100));

    /* ── Uygulama listesi ── */
    int y = my2+HEADER_H;
    for(int i=0;i<N_ITEMS;i++,y+=ROW_H){
        bool hov=(m.x>=mx2+8&&m.x<=mx2+mw-8&&m.y>=y&&m.y<y+ROW_H);
        if(hov) DrawRoundRect(mx2+8,y+1,mw-16,ROW_H-2,10,
                    LerpColor(WP_TitleTop(true),panel_bot,45,100));
        DrawRect(mx2+22,y+ROW_H/2-3,7,7,items[i].dot);
        DrawString(mx2+38,y+ROW_H/2-4,LS(items[i].id),
            hov?0xFFFFFF:LerpColor(WP_TitleText(true),0xFFFFFF,80,100));
    }

    /* ── Dev Tools — AYRI, ikinci bir panel (ana menünün sağında) ──
     * Eskiden bu bölüm ana panelin İÇİNDE, aynı sütunda alta ekleniyordu;
     * 10 ekstra satır ekranın kalan yüksekliğini aşınca panel kırpılıyor,
     * kırpılan sınırların dışında kalan satırlar görünür ama tıklanamaz
     * hale geliyordu. Ayrı, kendi başına yeterli yükseklikte bir panel
     * olunca bu taşma yapısal olarak imkansız. */
    if(g_dev_mode){
        int dmw = 220;
        int dmh = 28 + N_DEV*ROW_H;
        int dmx = mx2 + mw + 8;
        int dmy = my2 + mh - dmh; /* alt kenarları hizalı */
        if(dmy < 8) dmy = 8;

        DrawShadow(dmx,dmy,dmw,dmh);
        DrawRect(dmx,dmy,dmw,dmh,panel_bot);
        for(int row=0;row<dmh;row++){
            uint32_t c2=LerpColor(panel_top,panel_bot,row,dmh);
            int yy=dmy+row; if(yy<0||yy>=(int)SH) continue;
            for(int px=dmx;px<dmx+dmw;px++){
                if(px<0||px>=(int)SW) continue;
                backbuffer[yy*SW+px]=c2;
            }
        }
        DrawRectBorder(dmx,dmy,dmw,dmh,0xFF8800); /* turuncu çerçeve: dev-mode vurgusu */
        DrawString(dmx+12,dmy+8,"Developer Tools",0xFF8800);

        int dy=dmy+28;
        for(int i=0;i<N_DEV;i++,dy+=ROW_H){
            bool hov=(m.x>=dmx+6&&m.x<=dmx+dmw-6&&m.y>=dy&&m.y<dy+ROW_H);
            if(hov) DrawRect(dmx+4,dy+1,dmw-8,ROW_H-2,0x2A1400);
            DrawRect(dmx+16,dy+ROW_H/2-3,7,7,ddots[i]);
            DrawString(dmx+30,dy+ROW_H/2-4,dnames[i],hov?0xFFCC88:0xCC8844);
        }
    }

    /* ── Ayırıcı ── */
    DrawRect(mx2+16,y+SEP_H/2,mw-32,1,LerpColor(panel_top,0xFFFFFF,15,100));
    y += SEP_H;

    /* ── Restart ── */
    bool rhov=(m.x>=mx2+8&&m.x<=mx2+mw-8&&m.y>=y&&m.y<y+ROW_H);
    if(rhov) DrawRoundRect(mx2+8,y+1,mw-16,ROW_H-2,10,LerpColor(0x2266AA,panel_bot,30,100));
    DrawRect(mx2+22,y+ROW_H/2-3,7,7,0x60A5FA);
    DrawString(mx2+38,y+ROW_H/2-4,LS(S_RESTART),rhov?0xFFFFFF:0x8FC4FF);
    y+=ROW_H;

    /* ── Shutdown ── */
    bool shov2=(m.x>=mx2+8&&m.x<=mx2+mw-8&&m.y>=y&&m.y<y+ROW_H);
    if(shov2) DrawRoundRect(mx2+8,y+1,mw-16,ROW_H-2,10,LerpColor(0xCC2222,panel_bot,30,100));
    DrawRect(mx2+22,y+ROW_H/2-3,7,7,0xFF6060);
    DrawString(mx2+38,y+ROW_H/2-4,LS(S_SHUTDOWN),shov2?0xFFFFFF:0xFF9090);
}

/* ═══════════════════════════════════════════
   CONTEXT MENU
   ═══════════════════════════════════════════ */
static void DrawContextMenu(Mouse& m){
    if(!ctx_open) return;
    int mw=150,mh=90,mx2=ctx_x,my2=ctx_y;
    if(mx2+mw>SW) mx2=SW-mw;
    if(my2+mh>SH-42) my2=SH-42-mh;
    DrawShadow(mx2,my2,mw,mh);
    DrawGradientV(mx2,my2,mw,mh,TP().win_content,0xECE9D8);
    DrawRectBorder(mx2,my2,mw,mh,0x8080A0);
    StrID items[]={S_CTX_REFRESH,S_CTX_NEWFOLDER,S_CTX_PROPS};
    for(int i=0;i<3;i++){
        int iy=my2+2+i*29;
        bool hov=(m.x>=mx2+2&&m.x<=mx2+mw-2&&m.y>=iy&&m.y<=iy+28);
        if(hov) DrawGradientV(mx2+2,iy,mw-4,28,TP().title_foc_top,TP().title_foc_bot);
        DrawRect(mx2+2,iy+28,mw-4,1,0xCCCCC0);
        DrawString(mx2+10,iy+10,LS(items[i]),hov?TP().title_foc_text:0x000000);
    }
    (void)m;
}

/* ═══════════════════════════════════════════
   DESKTOP WALLPAPER
   ═══════════════════════════════════════════ */

/* ═══════════════════════════════════════════
   CURSOR
   ═══════════════════════════════════════════ */
static void DrawCursor(int mx,int my){
    PutPixel(mx+1,my+1,0x202020);
    for(int i=0;i<14;i++){
        int lw=(i<8)?i:13-i; if(lw<0)lw=0;
        DrawRect(mx,my+i,1+lw,1,0xFFFFFF);
        PutPixel(mx,my+i,0x000000);
        int e=(i<8)?i:13-i; if(e>0) PutPixel(mx+e,my+i,0x000000);
    }
}

/* ═══════════════════════════════════════════
   HARDWARE INIT
   ═══════════════════════════════════════════ */
static void EnableGfx(){
    /* kernel_main() zaten GRUB framebuffer'ı ya da BGA fallback'i
     * screen_w/screen_h/screen_pitch değerleriyle tutarlı şekilde kurdu.
     * Burada farklı sabit bir çözünürlüğe (800x600) geçmek o değerlerle
     * VRAM'i uyumsuz hale getirir (pitch kayması / kaymış ekran görünümü).
     * Bu yüzden BGA modunu mevcut screen_w/screen_h ile tekrar teyit ediyoruz,
     * hard-coded farklı bir değer kullanmıyoruz. */
    bga_set_mode((uint16_t)screen_w,(uint16_t)screen_h,32);
}
static void InitMouse(){
    hal_outb(0x64,0xA8);
    hal_outb(0x64,0x20); uint8_t st=hal_inb(0x60); st|=0x02;
    hal_outb(0x64,0x60); hal_outb(0x60,st);
    hal_outb(0x64,0xD4); hal_outb(0x60,0xF4); hal_inb(0x60);
}
static int TaskbarHit(Compositor& comp,int mx,int my){
    DockGeom dg = GetDockGeom();
    int dock_y=dg.y, dock_x=dg.x, dock_w=dg.w;
    if(my<dock_y||my>dock_y+dg.h) return -1;
    if(mx<dock_x||mx>dock_x+dock_w) return -1;
    int bx=dock_x+88+14; /* start buton genişliği(88) + separator boşluğu(14), DrawTaskbar ile aynı */
    int right_reserved=210;
    for(int i=0;i<comp.count;i++){
        if(!comp.Get(i)->visible) continue;
        if(mx>=bx&&mx<=bx+140) return i;
        bx+=146; if(bx+140>dock_x+dock_w-right_reserved) break;
    }
    return -1;
}
static void ApplyLang(Notepad& np,Calculator& calc,FileMgr& fm,Settings& st,Terminal& term){
    np.UpdateTitle(LS(S_NOTEPAD_TITLE));
    calc.UpdateLang(); fm.UpdateLang(); st.UpdateLang(); term.UpdateLang();
}

/* ═══════════════════════════════════════════
   ANA DÖNGÜ
   ═══════════════════════════════════════════ */
extern "C" void start_shell(){
    g_vfs.Init();
    acpi_ensure_init();
    /* Network: browser açılınca başlatılır */
    sound_init();
    /* ATA disk init + VFS yükle */
    serial_log("start_shell: calling disk_init()");
    bool dinit = disk_init();
    serial_puts(COM1, dinit ? "[LightOS] disk_init() = true\n" : "[LightOS] disk_init() = false\n");
    if(dinit){
        bool loaded = vfs_load();
        serial_puts(COM1, loaded ? "[LightOS] vfs_load() = true (CandleFS found)\n"
                                  : "[LightOS] vfs_load() = false (blank/unformatted disk)\n");
    }
    RunStartup(); /* Boot animasyonu + Dev Mode kontrolü */
    PlayStartup(); /* Açılış melodisi */
    EnableGfx(); InitMouse();

    static Mouse      mouse;
    static Keyboard   kbd;
    static Notepad    notepad;
    static Calculator calc;
    static FileMgr    filemgr;
    static Settings   settings;
    static Terminal   terminal;
    static Paint      paint;
    static SysInfo    sysinfo;
    static ClockApp   clockapp;
    static MemViewer    devMem;
    static PortTester   devPort;
    static VfsInspector devVfs;
    static CpuInfo      devCpu;
    static IrqMonitor   devIrq;
    static StackViewer  devStack;
    static KernelLog    devLog;
    static BsodTester   devBsod;
    static RtcInspector devRtc;
    static PanicSim     devPanic;
    static Browser      browser;
    static CalendarApp  calendar;
    static TaskManager  taskman;
    static GamesFolder  xaefgames;
    static LuigiAI      luigi;
    static HexEditor    hexed;
    static MusicPlayer  music;
    static Breakout     game_breakout;
    static Snake        game_snake;
    static Tetris       game_tetris;
    static Pong         game_pong;
    static Minesweeper  game_mines;

    static Compositor comp;

    comp.Add(&notepad); comp.Add(&calc); comp.Add(&filemgr);
    comp.Add(&settings); comp.Add(&terminal); comp.Add(&paint);
    comp.Add(&sysinfo); comp.Add(&clockapp);
    comp.Add(&browser);
    comp.Add(&calendar);
    comp.Add(&taskman);
    comp.Add(&xaefgames);
    comp.Add(&luigi);
    comp.Add(&hexed);
    comp.Add(&music);
    comp.Add(&game_breakout);
    comp.Add(&game_snake);
    comp.Add(&game_tetris);
    comp.Add(&game_pong);
    comp.Add(&game_mines);
    xaefgames.last_launch=-1;
    xaefgames.game_wins[0]=&game_snake;
    xaefgames.game_wins[1]=&game_tetris;
    xaefgames.game_wins[2]=&game_pong;
    xaefgames.game_wins[3]=&game_mines;
    g_comp_ptr = &comp;
    comp.Add(&devMem); comp.Add(&devPort); comp.Add(&devVfs);
    comp.Add(&devCpu); comp.Add(&devIrq); comp.Add(&devStack);
    comp.Add(&devLog); comp.Add(&devBsod); comp.Add(&devRtc);
    comp.Add(&devPanic);

    ApplyLang(notepad,calc,filemgr,settings,terminal);

    bool needs_redraw=true;
    bool prev_left=false,prev_right=false;
    bool ctrl_held=false;
    int  prev_kbd=g_kbd_layout;
    ThemeID prev_theme=g_theme;

    while(1){
        /* INPUT */
        uint8_t status=hal_inb(0x64);
        if(status&1){
            uint8_t data=hal_inb(0x60);
            if(status&0x20){
                bool pl=mouse.left,pr=mouse.right;
                mouse.Feed(data);
                if(mouse.has_moved||mouse.left!=pl||mouse.right!=pr) needs_redraw=true;
            } else {
                if(data==0x1D) ctrl_held=true;
                else if(data==0x9D) ctrl_held=false;
                else if(ctrl_held&&data==0x30&&g_dev_mode) BSOD(BSOD_UNEXPECTED_KERNEL);
                else {
                    unsigned int cp=kbd.UpdateCP(data);
                    if(cp){
                        Window* top=comp.Get(comp.count-1);
                        if(top&&top->visible&&!top->minimized){
                            if(top==(Window*)&notepad){ notepad.KeyPressCP(cp); needs_redraw=true; }
                            else if(top==(Window*)&filemgr){ if(cp<128){filemgr.KeyPress((char)cp);needs_redraw=true;} }
                            else if(top==(Window*)&terminal){ terminal.KeyPress(cp); needs_redraw=true; }
                        else if(top==(Window*)&devPanic){ devPanic.KeyPress(cp); needs_redraw=true; }
                        else if(top==(Window*)&devBsod){ devBsod.KeyPress(cp); needs_redraw=true; }
                        else if(top==(Window*)&browser){
                            browser.KeyPress(cp); needs_redraw=true;
                        }
                        else if(top==(Window*)&calendar){ calendar.KeyPress(cp); needs_redraw=true; }
                        else if(top==(Window*)&game_snake){ game_snake.KeyPress(cp); needs_redraw=true; }
                        else if(top==(Window*)&game_tetris){ game_tetris.KeyPress(cp); needs_redraw=true; }
                        else if(top==(Window*)&game_pong){ game_pong.KeyPress(cp); needs_redraw=true; }
                        else if(top==(Window*)&game_mines){ game_mines.KeyPress(cp); needs_redraw=true; }
                        else if(top==(Window*)&luigi){ luigi.KeyPress(cp); needs_redraw=true; }
                        else if(top==(Window*)&hexed){ hexed.KeyPress(cp); needs_redraw=true; }
                        else if(top==(Window*)&music){ music.KeyPress(cp); needs_redraw=true; }
                        else if(top==(Window*)&game_breakout){ game_breakout.KeyPress(cp); needs_redraw=true; }
                        }
                    }
                }
            }
        }

        /* CHANGE DETECT */
        if(g_kbd_layout!=prev_kbd||g_theme!=prev_theme){
            if(g_kbd_layout!=prev_kbd){g_lang=(g_kbd_layout==1)?LANG_TR:LANG_EN;prev_kbd=g_kbd_layout;}
            prev_theme=g_theme;
            ApplyLang(notepad,calc,filemgr,settings,terminal);
            needs_redraw=true;
        }

        bool clicked=mouse.left&&!prev_left;
        bool rclicked=mouse.right&&!prev_right;
        prev_left=mouse.left; prev_right=mouse.right;

        Window* all_wins[]={&notepad,&filemgr,&calc,&settings,&terminal,&paint,&sysinfo,&clockapp,&browser,&calendar,&taskman,&xaefgames,&luigi,&hexed,&music};

        if(clicked){
            ctx_open=false;
            DockGeom dg2 = GetDockGeom();
            int dock_y2=dg2.y, dock_x2=dg2.x, dock_w2=dg2.w;
            /* Bu koordinatlar DrawTaskbar() ile BİREBİR aynı olmalı:
             * qx=dock_x+2, qw=88 (Start butonu); tray_x=dock_x+dock_w-
             * right_reserved+8, lx=tray_x, lw=40 (dil butonu). */
            int right_reserved=210;
            int start_x0=dock_x2+2, start_x1=start_x0+88;
            int tray_x2=dock_x2+dock_w2-right_reserved+8;
            int lang_x0=tray_x2, lang_x1=lang_x0+40;
            if(mouse.y>=dock_y2&&mouse.x>=start_x0&&mouse.x<=start_x1){
                qmenu_open=!qmenu_open; needs_redraw=true;
            } else if(mouse.y>=dock_y2&&mouse.x>=lang_x0&&mouse.x<=lang_x1){
                g_kbd_layout=(g_kbd_layout==0)?1:0;
                g_lang=(g_kbd_layout==1)?LANG_TR:LANG_EN;
                prev_kbd=g_kbd_layout;
                ApplyLang(notepad,calc,filemgr,settings,terminal);
                qmenu_open=false; needs_redraw=true;
            } else if(mouse.y>=dock_y2){
                int ti=TaskbarHit(comp,mouse.x,mouse.y);
                if(ti>=0){
                    Window* ww=comp.Get(ti);
                    if(ww->minimized){ww->minimized=false;comp.BringToFront(ti);}
                    else if(ti==comp.count-1) ww->minimized=true;
                    else comp.BringToFront(ti);
                    qmenu_open=false; needs_redraw=true;
                }
            } else if(qmenu_open){
                /* Bu blok DrawQuickMenu() ile AYNI geometriyi kullanmalı,
                 * aksi halde menü bir yerde görünür ama tıklamalar başka
                 * yere denk gelir (daha önce Start-menu butonunda yaşanan
                 * aynı sınıf bug). Sabitler DrawQuickMenu ile birebir
                 * eşleşiyor: ROW_H, HEADER_H, SEP_H, mw, ve konum hesabı.
                 * Dev Tools artık ayrı bir sağ panelde (bkz. DrawQuickMenu
                 * yorumu) — sol panel her zaman sabit N_ITEMS satırlık,
                 * taşma yapısal olarak imkansız. */
                const int ROW_H=30, HEADER_H=56, SEP_H=9;
                int mw=264;
                const int N_ITEMS=14, N_DEV=10;
                int mh = HEADER_H + N_ITEMS*ROW_H + SEP_H + 2*ROW_H + 10;
                DockGeom dgm = GetDockGeom();
                int mx2 = dgm.x;
                int my2 = dgm.y - mh - 10;
                if(my2<8) my2=8;
                int max_h = SH - my2 - 8;
                if(mh>max_h) mh=max_h;

                /* Normal uygulamalar (sol panel) */
                int app_map[]={0,2,1,3,4,5,6,7,8,9,10,12,13,14};
                Window* all_wins2[]={&notepad,&filemgr,&calc,&settings,
                                     &terminal,&paint,&sysinfo,&clockapp,&browser,&calendar,&taskman,&xaefgames,&luigi,&hexed,&music};
                int y=my2+HEADER_H;
                bool inside_left = mouse.x>=mx2&&mouse.x<=mx2+mw&&mouse.y>=my2&&mouse.y<=my2+mh;
                for(int i=0;i<N_ITEMS;i++,y+=ROW_H){
                    if(mouse.x>=mx2+8&&mouse.x<=mx2+mw-8&&mouse.y>=y&&mouse.y<y+ROW_H){
                        Window* t=all_wins2[app_map[i]];
                        if(!t->visible||t->minimized) t->Open();
                        for(int j=0;j<comp.count;j++) if(comp.Get(j)==t){comp.BringToFront(j);break;}
                        qmenu_open=false; needs_redraw=true;
                    }
                }

                y+=SEP_H;
                if(mouse.x>=mx2+8&&mouse.x<=mx2+mw-8&&mouse.y>=y&&mouse.y<y+ROW_H) DoRestart();
                y+=ROW_H;
                if(mouse.x>=mx2+8&&mouse.x<=mx2+mw-8&&mouse.y>=y&&mouse.y<y+ROW_H) DoShutdown();

                /* Dev Tools tıklaması (sağ panel) — DrawQuickMenu'deki
                 * dmw/dmh/dmx/dmy hesabıyla birebir aynı. */
                bool inside_right=false;
                if(g_dev_mode){
                    int dmw=220;
                    int dmh=28+N_DEV*ROW_H;
                    int dmx=mx2+mw+8;
                    int dmy=my2+mh-dmh;
                    if(dmy<8) dmy=8;
                    inside_right = mouse.x>=dmx&&mouse.x<=dmx+dmw&&mouse.y>=dmy&&mouse.y<=dmy+dmh;
                    Window* dev_wins[]={&devMem,&devPort,&devVfs,&devCpu,
                                        &devIrq,&devStack,&devLog,&devBsod,
                                        &devRtc,&devPanic};
                    int dy=dmy+28;
                    for(int i=0;i<N_DEV;i++,dy+=ROW_H){
                        if(mouse.x>=dmx+6&&mouse.x<=dmx+dmw-6&&mouse.y>=dy&&mouse.y<dy+ROW_H){
                            Window* t=dev_wins[i];
                            if(!t->visible||t->minimized) t->Open();
                            for(int j=0;j<comp.count;j++) if(comp.Get(j)==t){comp.BringToFront(j);break;}
                            qmenu_open=false; needs_redraw=true;
                        }
                    }
                }

                if(!inside_left && !inside_right) qmenu_open=false;
            } else {
                /* Önce dev ikonları kontrol et (sağ tarafta — pencere altında kalabilir) */
                bool dev_icon_hit=false;
                if(g_dev_mode){
                    Window* dev_wins[]={&devMem,&devPort,&devVfs,&devCpu,
                                        &devIrq,&devStack,&devLog,&devBsod,
                                        &devRtc,&devPanic};
                    /* DrawDesktopIcons() ile BİREBİR aynı sabitler —
                     * aksi halde ikon bir yerde görünür, tıklama başka
                     * yere denk gelir. */
                    const int DEV_ICON_W=44, DEV_COL_GAP=72, DEV_ROW_GAP=78;
                    for(int i=0;i<10;i++){
                        int col=i%2, row=i/2;
                        int dix=SW-8-DEV_COL_GAP*2+col*DEV_COL_GAP;
                        int diy=8+row*DEV_ROW_GAP;
                        if(mouse.x>=dix-2&&mouse.x<=dix+DEV_ICON_W+2&&
                           mouse.y>=diy-2&&mouse.y<=diy+DEV_ROW_GAP-8){
                            Window* t=dev_wins[i];
                            /* Pencereyi ekran ortasına konumlandır */
                            t->x = (SW - t->w) / 2;
                            t->y = (SH - 40 - t->h) / 2;
                            if(!t->visible||t->minimized) t->Open();
                            else { t->visible=true; t->minimized=false; }
                            for(int j=0;j<comp.count;j++)
                                if(comp.Get(j)==t){comp.BringToFront(j);break;}
                            dev_icon_hit=true;
                        }
                    }
                }
                /* Normal masaüstü ve pencere tıklamaları */
                if(!dev_icon_hit && !comp.HandleClick(mouse.x,mouse.y,true)){
                    for(int i=0;i<ICON_N;i++){
                        DeskIcon& ic=desk_icons[i];
                        int ix2,iy2; icon_pos(i,ix2,iy2);
                        if(mouse.x>=ix2&&mouse.x<=ix2+ICON_W&&
                           mouse.y>=iy2&&mouse.y<=iy2+ICON_H){
                            Window* t=all_wins[ic.app_id];
                            if(!t->visible||t->minimized) t->Open();
                            for(int j=0;j<comp.count;j++) if(comp.Get(j)==t){comp.BringToFront(j);break;}
                        }
                    }
                } else if(!dev_icon_hit){
                    /* comp.HandleClick handled it */
                }
            }
            needs_redraw=true;
        }
        if(rclicked&&mouse.y<SH-40&&comp.HitTest(mouse.x,mouse.y)<0){
            ctx_open=true;ctx_x=mouse.x;ctx_y=mouse.y;qmenu_open=false;needs_redraw=true;
        }
        /* Paint: fare basılıyken sürekli çizim */
        if(mouse.left && comp.Get(comp.count-1)==(Window*)&paint)
            paint.HandleClick(mouse.x,mouse.y,true);
        comp.UpdateDrag(mouse.x,mouse.y,mouse.left);

        /* FileMgr -> Notepad dosya açma isteği */
        if(filemgr.open_request_node >= 0){
            notepad.OpenVfsNode(filemgr.open_request_node);
            filemgr.open_request_node = -1;
            if(!notepad.visible || notepad.minimized) notepad.Open();
            for(int j=0;j<comp.count;j++)
                if(comp.Get(j)==(Window*)&notepad){comp.BringToFront(j);break;}
            needs_redraw=true;
        }

        /* DRAW */
        if(needs_redraw){
            DrawWallpaper();
            DrawDesktopIcons(mouse);
            comp.Draw();
            DrawTaskbar(comp,mouse);
            DrawQuickMenu(mouse);
            DrawContextMenu(mouse);
            DrawCursor(mouse.x,mouse.y);
            mouse.has_moved=false;
            SwapAll();
            needs_redraw=false;
        }
        /* Game ticks — PIT Channel 0 tabanli (~18.2Hz, CPU-independent) */
        {
            uint16_t _cur = pit_read_count();
            if(_cur > g_pit_last) g_pit_ticks++;  /* wrap = ~54.9ms gecti */
            g_pit_last = _cur;
            static uint32_t last_game_tick = 0xFFFFFFFF;
            if(g_pit_ticks != last_game_tick){
                last_game_tick = g_pit_ticks;
                if(game_snake.visible&&!game_snake.minimized){game_snake.Tick();needs_redraw=true;}
                if(game_tetris.visible&&!game_tetris.minimized){game_tetris.Tick();needs_redraw=true;}
                if(game_pong.visible&&!game_pong.minimized){game_pong.Tick();needs_redraw=true;}
        if(game_breakout.visible&&!game_breakout.minimized){game_breakout.Tick();needs_redraw=true;}
        if(music.visible&&!music.minimized){music.Tick();needs_redraw=true;}
            }
            /* ── Genel "heartbeat" redraw ──
             * Yukarıdaki needs_redraw tetiklemeleri sadece belirli oyun
             * pencereleri açıkken çalışıyordu. Mouse hiç hareket etmez
             * ve hiçbir oyun açık değilse needs_redraw bir daha ASLA
             * true olmuyordu — bu da saatin ilerlememesi, Rain duvar
             * kağıdındaki yağmurun donması, ve genel olarak "sistem
             * kilitlenmiş gibi" görünmesine yol açıyordu (mouse'u
             * kıpırdatınca her şey normale dönüyordu, çünkü input
             * geldiğinde needs_redraw zaten ayrıca true oluyordu).
             * Burada, kullanıcı hiçbir şey yapmasa bile saniyede birkaç
             * kez ekranı tazeleyen düşük frekanslı bir sayaç var — bu
             * saati ve arka plan animasyonlarını canlı tutar, ama gereksiz
             * yere CPU'yu (mouse'un idle olduğu her mikrosaniyede) meşgul
             * etmez. ~18.2Hz PIT tikinden 4'te 1 alınarak ~4.5Hz'e
             * düşürülüyor — saat/animasyon için fazlasıyla yeterli, göz
             * ile fark edilecek bir gecikme yaratmıyor. */
            static uint32_t last_heartbeat = 0xFFFFFFFF;
            if((g_pit_ticks>>2) != (last_heartbeat>>2)){
                last_heartbeat = g_pit_ticks;
                needs_redraw = true;
            }
        }
        /* XaefGAMES launch focus */
        if(xaefgames.last_launch>=0){
            Window* lw=xaefgames.game_wins[xaefgames.last_launch];
            if(lw) for(int _i=0;_i<comp.count;_i++) if(comp.Get(_i)==lw){comp.BringToFront(_i);break;}
            xaefgames.last_launch=-1; needs_redraw=true;
        }
        /* Clock: Her frame'de zaman çıktısı (sadece taskbar redraw) */
    }
}
