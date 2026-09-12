#ifndef WALLPAPER_H
#define WALLPAPER_H
/*
 * wallpaper.h — LightOS Masaüstü Duvar Kağıdı
 * 3 farklı tema:
 *   0 = Luna Gece (varsayılan — koyu mavi halkalar)
 *   1 = Şafak     (turuncu-mor gradient)
 *   2 = Orman     (koyu yeşil)
 */
#include "graphics.h"
#include "theme.h"
#include "font.h"
#include "kernel/wallpaper_ppm.h"

extern int g_wallpaper;  /* shell.cpp'de tanımlanır */

static void DrawScaledTextWP(const char* msg,int bx,int by,int scale,uint32_t color){
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

/* Yıldız helper */
static void DrawStars(int count,uint32_t col,unsigned int seed){
    for(int i=0;i<count;i++){
        seed=seed*1664525+1013904223;
        int sx=(int)(seed%SW);
        seed=seed*1664525+1013904223;
        int sy=(int)((seed>>1)%((SH-40)*4/5));
        PutPixel(sx,sy,col);
        if(i%3==0) PutPixel(sx+1,sy,col);
    }
}

/* Yağmur damlası state — gerçek zamana (PIT tick, ~18.2Hz, CPU/frame-rate
 * bağımsız) göre ilerler. Önceden g_rain_tick her DrawWallpaper() çağrısında
 * (yani her frame'de) bir artıyordu — mouse hareket ederken çok daha sık
 * redraw olduğu için yağmur hızlı akıyor, mouse dururken (heartbeat ~4.5Hz'e
 * düştüğünde) yağmur gözle görülür şekilde yavaşlıyordu. PIT tick'e
 * bağlanınca animasyon hızı artık ekranın ne sıklıkta yeniden çizildiğinden
 * tamamen bağımsız, her koşulda sabit gerçek-zaman hızında akıyor. */
extern uint32_t g_pit_ticks;
#define RAIN_DROP_COUNT 90

static void DrawRainWallpaper(){
    int dh=SH;
    /* Yumuşak, göz yormayan gece-mavisi gökyüzü — Luna Gece'den daha
     * nötr/gri tonlu, "rahat" hissi için parlaklık düşük tutuldu. */
    for(int row=0;row<dh;row++){
        uint32_t c=LerpColor(0x10161E,0x232C38,row,dh);
        DrawRect(0,row,SW,1,c);
    }

    /* Uzak şehir silueti — düz, sakin bina blokları, birkaçında sıcak
     * pencere ışığı (statik, sarı/turuncu — yağmurun soğukluğuna karşı
     * sıcak bir denge, "eve dönmüş gibi" bir his için). */
    unsigned int bseed=0x9E3779B9u;
    int baseline=dh-40;
    int bx=0;
    while(bx<(int)SW){
        bseed=bseed*1664525+1013904223;
        int bw=40+(int)(bseed%50);
        bseed=bseed*1664525+1013904223;
        int bh=60+(int)(bseed%140);
        int by2=baseline-bh;
        DrawRect(bx,by2,bw,bh,0x181E28);
        /* pencereler */
        for(int wy=by2+8; wy<baseline-8; wy+=14){
            for(int wx=bx+6; wx<bx+bw-6; wx+=12){
                bseed=bseed*1664525+1013904223;
                bool lit=(bseed%5)==0;
                DrawRect(wx,wy,5,7, lit?0xC89A4A:0x10141A);
            }
        }
        bx+=bw+6;
    }
    DrawRect(0,baseline,SW,dh-baseline,0x0C0F14);

    /* Yağmur damlaları — deterministik seed'den üretilip g_pit_ticks'e
     * göre aşağı kaydırılır. Farklı "derinlik katmanları" (yavaş/uzak,
     * hızlı/yakın) hafif bir paralaks hissi verir. */
    unsigned int rseed=0x1234ABCDu;
    for(int i=0;i<RAIN_DROP_COUNT;i++){
        rseed=rseed*1664525+1013904223;
        int rx=(int)(rseed%SW);
        rseed=rseed*1664525+1013904223;
        int speed=2+(int)(rseed%3);           /* 2-4 px/frame */
        int len=8+speed*3;                     /* hızlı damla = uzun iz */
        rseed=rseed*1664525+1013904223;
        int phase=(int)(rseed%(uint32_t)(dh+len));
        int ry=(int)((phase + g_pit_ticks*speed*4) % (uint32_t)(dh+len)) - len;
        uint32_t col = speed>=4 ? 0x8FA8C0 : 0x5C6E82; /* yakın damla daha parlak */
        for(int l=0;l<len;l++){
            int yy=ry+l;
            if(yy<0||yy>=dh) continue;
            int xx=rx-l/4; /* hafif eğik düşüş */
            if(xx<0||xx>=(int)SW) continue;
            uint32_t a = LerpColor(col,0x232C38,l*60/len,100);
            PutPixel(xx,yy,a);
        }
    }

    DrawScaledTextWP("LightOS",SW/2-3*18,20,2,LerpColor(0x232C38,0xC8D4E0,45,100));
}

static void DrawWallpaper(){
    int dh=SH;
    switch(g_wallpaper){

    /* ─── 4: Rain — sakin, karanlık şehir + animasyonlu yağmur ─── */
    case 4: {
        DrawRainWallpaper();
        break;
    }

    /* ─── 3: Özel (kullanıcının diske yüklediği PPM fotoğraf) ───
     * Diskte kayıtlı bir PPM bulunamazsa (yüklenmemiş, disk yok, bozuk
     * dosya vb.) sessizce Luna Gece'ye düşer — hiçbir zaman boş/siyah
     * ekran kalmaz. */
    case 3: {
        if(!wp_ppm_draw()){
            g_wallpaper=0;
            DrawWallpaper();
            return;
        }
        break;
    }

    /* ─── 0: Luna Gece ─── */
    case 0: default: {
        for(int row=0;row<dh;row++){
            int half=dh/2;
            uint32_t c=row<half
                ?LerpColor(TP().desk_top,BrightColor(TP().desk_top,120),row,half)
                :LerpColor(BrightColor(TP().desk_top,120),TP().desk_bot,row-half,dh-half);
            DrawRect(0,row,SW,1,c);
        }
        /* Işık bandı */
        int by2=dh*2/5;
        for(int i=0;i<18;i++){
            int a=(i<9)?i:17-i;
            DrawRect(0,by2+i,SW,1,LerpColor(TP().desk_top,BrightColor(TP().desk_bot,150),a,8));
        }
        /* Luna halkası */
        int cx2=SW*3/4,cy2=dh/2-10,cr=120;
        for(int dy=-cr;dy<=cr;dy++){
            int dx2=cr*cr-dy*dy; if(dx2<0) continue;
            int dx=1; while(dx*dx<=dx2) dx++; dx--;
            for(int xi=-dx;xi<=dx;xi++){
                int rimD=dx-(xi<0?-xi:xi);
                if(rimD<12){
                    int px2=cx2+xi,py2=cy2+dy;
                    if(px2<0||py2<0||px2>=(int)SW||py2>=dh) continue;
                    int a=(12-rimD)*10; if(a>70)a=70;
                    uint32_t base=backbuffer[py2*SW+px2];
                    backbuffer[py2*SW+px2]=LerpColor(base,0x90CCF0,a,100);
                }
            }
        }
        DrawScaledTextWP("LightOS",SW*3/4-3*18,dh/2+cr-22,2,LerpColor(TP().desk_top,0x80C0FF,30,100));
        DrawStars(80,0xC0D8EE,0xDEAD1234);
        break;
    }

    /* ─── 1: Şafak ─── */
    case 1: {
        for(int row=0;row<dh;row++){
            uint32_t c;
            if(row<dh/3)      c=LerpColor(0x1A0830,0x8B1A6A,row,dh/3);
            else if(row<dh*2/3) c=LerpColor(0x8B1A6A,0xE8642A,row-dh/3,dh/3);
            else              c=LerpColor(0xE8642A,0xFFCC44,row-dh*2/3,dh/3);
            DrawRect(0,row,SW,1,c);
        }
        /* Güneş diski */
        int sx2=SW/2,sy2=dh*2/3;
        for(int dy=-40;dy<=40;dy++)
            for(int dx=-40;dx<=40;dx++)
                if(dx*dx+dy*dy<=1600){
                    int bri=100-(dx*dx+dy*dy)*100/1600;
                    uint32_t s=LerpColor(0xFFCC00,0xFFFFFF,bri,100);
                    if(sx2+dx>=0&&sy2+dy>=0&&sx2+dx<(int)SW&&sy2+dy<dh)
                        PutPixel(sx2+dx,sy2+dy,s);
                }
        /* Güneş ışınları */
        for(int a=0;a<360;a+=20){
            int len=60+a%3*10;
            for(int l=45;l<len;l++){
                int px2=sx2+l*(a%180<90?(a<90?1:-1):1)/2;
                int py2=sy2+l*(a<180?-1:1)/3;
                if(px2>=0&&py2>=0&&px2<(int)SW&&py2<dh)
                    PutPixel(px2,py2,LerpColor(0xFFCC00,0x00000000&0xFF8800,l-45,len-45));
            }
        }
        /* Ufuk yansıması */
        for(int row=dh*2/3;row<dh;row++)
            DrawRect(0,row,SW,1,LerpColor(0x601020,0x180408,row-dh*2/3,dh/3));
        DrawScaledTextWP("LightOS",SW/2-3*18,dh-50,2,0xFFEE88);
        DrawStars(40,0xFFEE99,0xABCD5678);
        break;
    }

    /* ─── 2: Orman ─── */
    case 2: {
        for(int row=0;row<dh;row++){
            uint32_t c=row<dh/2
                ?LerpColor(0x040C08,0x083018,row,dh/2)
                :LerpColor(0x083018,0x0C4820,row-dh/2,dh/2);
            DrawRect(0,row,SW,1,c);
        }
        /* Ağaç siluetleri */
        unsigned int tseed=0x12345678;
        for(int t=0;t<12;t++){
            tseed=tseed*1664525+1013904223;
            int tx2=(int)(tseed%SW);
            tseed=tseed*1664525+1013904223;
            int th2=80+(int)(tseed%80);
            int ty2=dh-th2;
            /* gövde */
            DrawRect(tx2-2,ty2+th2/2,4,th2/2,0x1A0A04);
            /* yaprak */
            for(int ly=0;ly<th2/2;ly++){
                int lw=(th2/2-ly)*th2/200+2;
                DrawRect(tx2-lw,ty2+ly,lw*2,1,LerpColor(0x082010,0x204830,ly,th2/2));
            }
        }
        /* Ay */
        int mx2=SW-120,my2=60;
        for(int dy=-30;dy<=30;dy++)
            for(int dx=-30;dx<=30;dx++)
                if(dx*dx+dy*dy<=900){
                    if(mx2+dx>=0&&my2+dy>=0&&mx2+dx<(int)SW&&my2+dy<dh)
                        PutPixel(mx2+dx,my2+dy,LerpColor(0xCCDDCC,0xEEFFEE,dx*dx+dy*dy,900));
                }
        DrawScaledTextWP("LightOS",SW/2-3*18,dh-40,2,0x40CC60);
        DrawStars(100,0x80CC88,0x11223344);
        break;
    }
    }
}
/* ─── Duvar kağıdına göre başlık çubuğu renkleri ─── */
struct WPAccent {
    uint32_t title_foc_top;
    uint32_t title_foc_bot;
    uint32_t title_foc_shine;
    uint32_t title_foc_text;
    uint32_t title_unf_top;
    uint32_t title_unf_bot;
    uint32_t title_unf_text;
    uint32_t win_frame_foc;
};

static const WPAccent WP_ACCENTS[5] = {
    /* 0: Luna Night — tema renkleri baskın (TP() kullanılır) */
    { 0,0,0,0, 0,0,0, 0 },
    /* 1: Dawn / Şafak — sıcak mor-turuncu */
    { 0x6B1060, 0xC8420A, 0xFF9966, 0xFFEEDD,
      0x4A0840, 0x8B2A0A, 0xDDB8A0,
      0xC8420A },
    /* 2: Orman — koyu yeşil */
    { 0x0A3018, 0x1A6030, 0x44CC66, 0xCCFFDD,
      0x082010, 0x104020, 0x88BB99,
      0x1A6030 },
    /* 3: Custom/PPM — kullanılmıyor (WP_TitleTop vb. bu index'e hiç
     * gelmeden g_wallpaper==3 için tema varsayılanına yönlendiriyor),
     * dizi hizası bozulmasın diye yer tutucu olarak duruyor. */
    { 0,0,0,0, 0,0,0, 0 },
    /* 4: Rain — yumuşak gece-mavisi, düşük kontrast ("rahat" his için
     * parlak/doygun renklerden kaçınıldı, gözü yormayan gri-mavi tonlar) */
    { 0x2A3644, 0x1C2530, 0x7C93A8, 0xDCE4EC,
      0x1C242E, 0x141A22, 0x8896A4,
      0x3A4A5C },
};

/* Başlık rengi al: tema + duvar kağıdı karışımı.
 * g_wallpaper==3 (Custom/PPM fotoğraf) için WP_ACCENTS dizisinde özel bir
 * anlamlı girdi yok (fotoğraf üstüne otomatik "accent" rengi hesaplamak
 * anlamsız olacağından) — Custom modda tema varsayılanına (0 ile aynı
 * davranış) düşülür. g_wallpaper==4 (Rain) ise WP_ACCENTS[4]'te kendi
 * yumuşak gece-mavisi accent setine sahip, genel yola düşer. */
static inline uint32_t WP_TitleTop(bool focused){
    if(g_wallpaper==0||g_wallpaper==3) return focused?TP().title_foc_top:TP().title_unf_top;
    const WPAccent& a=WP_ACCENTS[g_wallpaper];
    return focused?a.title_foc_top:a.title_unf_top;
}
static inline uint32_t WP_TitleBot(bool focused){
    if(g_wallpaper==0||g_wallpaper==3) return focused?TP().title_foc_bot:TP().title_unf_bot;
    const WPAccent& a=WP_ACCENTS[g_wallpaper];
    return focused?a.title_foc_bot:a.title_unf_bot;
}
static inline uint32_t WP_TitleShine(){
    if(g_wallpaper==0||g_wallpaper==3) return TP().title_foc_shine;
    return WP_ACCENTS[g_wallpaper].title_foc_shine;
}
static inline uint32_t WP_TitleText(bool focused){
    if(g_wallpaper==0||g_wallpaper==3) return focused?TP().title_foc_text:TP().title_unf_text;
    const WPAccent& a=WP_ACCENTS[g_wallpaper];
    return focused?a.title_foc_text:a.title_unf_text;
}
static inline uint32_t WP_FrameFoc(){
    if(g_wallpaper==0||g_wallpaper==3) return TP().win_frame_foc;
    return WP_ACCENTS[g_wallpaper].win_frame_foc;
}
#endif
