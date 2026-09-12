#ifndef SCREENSHOT_H
#define SCREENSHOT_H
/*
 * apps/screenshot.h — Ekran Görüntüsü (BMP)
 * =============================================
 * Backbuffer'ı 24-bit BMP olarak VFS'e kaydeder.
 * Ctrl+Print veya Screenshot butonuyla tetiklenir.
 * Dosya: Screenshots/screen_N.bmp
 * Maksimum: 9 screenshot (VFS sınırı)
 */
#include "../gui.h"
#include "../kernel/vfs.h"

extern uint32_t* backbuffer;
extern uint32_t  screen_w, screen_h;

struct BMPHeader {
    /* File header */
    uint8_t  sig[2];
    uint32_t file_size;
    uint32_t reserved;
    uint32_t data_offset;
    /* DIB header (BITMAPINFOHEADER) */
    uint32_t dib_size;
    int32_t  width, height;
    uint16_t planes, bpp;
    uint32_t compression, image_size;
    int32_t  x_ppm, y_ppm;
    uint32_t colors_used, colors_important;
} __attribute__((packed));

/* VFS tek dosyaya 4096 byte sığdırıyor.
   800×600×3 = 1.44MB >> 4096.
   Küçük thumbnail BMP yaz: 64×48 (9216 byte > 4096 yine büyük).
   32×24 = 2304 byte + header ≈ 2358 byte — sığar! */
static const int SS_W = 32, SS_H = 24;  /* thumbnail boyutu */

static int g_screenshot_count = 0;

static bool take_screenshot(){
    /* Screenshots klasörünü bul/oluştur */
    int dir=g_vfs.FindChild(0,"Screenshots");
    if(dir<0) dir=g_vfs.MkDir(0,"Screenshots");
    if(dir<0) return false;

    /* Dosya adı */
    char fname[20]="screen_0.bmp";
    fname[7]=(char)('0'+g_screenshot_count%9);
    g_screenshot_count++;

    /* Küçük BMP verisi oluştur */
    /* Satır başına 3 byte, 4'e hizalanmış */
    int row_bytes=((SS_W*3+3)/4)*4;
    int image_bytes=row_bytes*SS_H;
    int total=54+image_bytes;

    /* VFS sınırı 4096 — kontrol */
    if(total>VFS_MAX_DATA) return false;

    /* Static buffer (stack'e sığmaz) */
    static uint8_t buf[VFS_MAX_DATA];
    for(int i=0;i<total;i++) buf[i]=0;

    /* BMP header */
    buf[0]='B'; buf[1]='M';
    *(uint32_t*)(buf+2) =(uint32_t)total;
    *(uint32_t*)(buf+6) =0;
    *(uint32_t*)(buf+10)=54;
    *(uint32_t*)(buf+14)=40;
    *(int32_t*) (buf+18)=(int32_t)SS_W;
    *(int32_t*) (buf+22)=(int32_t)SS_H;
    *(uint16_t*)(buf+26)=1;
    *(uint16_t*)(buf+28)=24;
    *(uint32_t*)(buf+30)=0;
    *(uint32_t*)(buf+34)=(uint32_t)image_bytes;

    /* Piksel verileri — BMP alt satırdan başlar */
    int sw=(int)screen_w, sh=(int)screen_h;
    for(int py=0;py<SS_H;py++){
        for(int px=0;px<SS_W;px++){
            /* Orijinal pikseli örnekle */
            int ox=px*sw/SS_W, oy=py*sh/SS_H;
            uint32_t color=backbuffer[(sh-1-oy)*sw+ox];
            int idx=54+(SS_H-1-py)*row_bytes+px*3;
            buf[idx+0]=(uint8_t)(color&0xFF);       /* B */
            buf[idx+1]=(uint8_t)((color>>8)&0xFF);  /* G */
            buf[idx+2]=(uint8_t)((color>>16)&0xFF); /* R */
        }
    }

    /* VFS'e yaz — ham BMP verisi \0 byte'ları içerebileceğinden
     * WriteFile() (strlen tabanlı) yerine WriteFileBytes() kullanılıyor. */
    int existing=g_vfs.FindChild(dir,fname);
    if(existing>=0){
        g_vfs.WriteFileBytes(existing, buf, total);
        return true;
    }
    /* Yeni dosya oluştur, sonra gerçek BMP içeriğini yaz */
    int node=g_vfs.MkFile(dir,fname,"");
    if(node<0) return false;
    g_vfs.WriteFileBytes(node, buf, total);
    return true;
}
#endif
