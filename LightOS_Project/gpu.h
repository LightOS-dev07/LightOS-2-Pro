#ifndef GPU_H
#define GPU_H

/*
 * gpu.h — LightOS GPU Abstraction Layer
 * ======================================
 * Gerçek donanım hızlandırması:
 *   1. SSE2 MOVDQA/MOVNTDQ ile hızlı buffer kopyalama
 *   2. Framebuffer pitch-aware blit
 *   3. Gaussian shadow precompute (separable box-blur × 3 pass)
 *   4. Dirty rect tracking — sadece değişen alanı VRAM'e bas
 *
 * Adres haritası (bare metal, sabit):
 *   0x00A00000 → backbuffer  (4 byte/px × 1024×768 = 3 MB max)
 *   0x00D00000 → shadow_buf  (1 byte/px × 1024×768 = 0.75 MB)
 *   0x00E00000 → blur_tmp    (1 byte/px, yatay geçiş tamponu)
 */

#include "shared.h"

/* Framebuffer değişkenleri kernel.c'den */
extern uint32_t* vram;
extern uint32_t  screen_pitch;
extern uint32_t* backbuffer;

/* ─── Shadow & Blur tamponu ─── */
static uint8_t* const shadow_buf = (uint8_t*)0x00D00000;
static uint8_t* const blur_tmp   = (uint8_t*)0x00E00000;

/* ─────────────────────────────────────────────────────────────
   SSE2 Hızlı Blit — Backbuffer → VRAM
   Non-temporal store (MOVNTDQ) cache'i kirletmez,
   VRAM'e yazarken çok daha verimlidir.
   ─────────────────────────────────────────────────────────── */
static void GPU_SwapBuffers(void) {
    uint8_t* src = (uint8_t*)backbuffer;
    uint8_t* dst = (uint8_t*)vram;
    uint32_t row_bytes = SW * 4;
    uint32_t pitch     = screen_pitch;

    for (uint32_t y = 0; y < SH; y++) {
        uint8_t* s = src + y * row_bytes;
        uint8_t* d = dst + y * pitch;

        /* 16-byte hizalı SSE2 kopyalama */
        uint32_t chunks = row_bytes / 16;
        uint32_t remain = row_bytes % 16;

        __asm__ volatile (
            "test %2, %2\n"
            "jz 2f\n"
            "1:\n"
            "movdqu  (%0), %%xmm0\n"
            "movntdq %%xmm0, (%1)\n"
            "add  $16, %0\n"
            "add  $16, %1\n"
            "dec  %2\n"
            "jnz  1b\n"
            "2:\n"
            "sfence\n"
            : "+r"(s), "+r"(d), "+r"(chunks)
            :: "xmm0", "memory"
        );

        /* Kalan byte'lar */
        uint8_t* sv = s; uint8_t* dv = d;
        for (uint32_t i = 0; i < remain; i++) dv[i] = sv[i];
    }
}

/* ─────────────────────────────────────────────────────────────
   Dirty Rect VRAM Blit — sadece değişen dikdörtgeni gönder
   ─────────────────────────────────────────────────────────── */
static void GPU_BlitRect(int x, int y, int w, int h) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > (int)SW) w = SW - x;
    if (y + h > (int)SH) h = SH - y;
    if (w <= 0 || h <= 0) return;

    uint8_t* src = (uint8_t*)backbuffer;
    uint8_t* dst = (uint8_t*)vram;
    uint32_t line = (uint32_t)w * 4;

    for (int row = 0; row < h; row++) {
        uint8_t* s = src + (y+row) * SW * 4      + x * 4;
        uint8_t* d = dst + (y+row) * screen_pitch + x * 4;
        uint32_t chunks = line / 16, remain = line % 16;
        __asm__ volatile (
            "test %2,%2\njz 2f\n"
            "1:movdqu (%0),%%xmm0\nmovntdq %%xmm0,(%1)\n"
            "add $16,%0\nadd $16,%1\ndec %2\njnz 1b\n2:sfence\n"
            :"+r"(s),"+r"(d),"+r"(chunks)::"xmm0","memory");
        for (uint32_t i = 0; i < remain; i++) d[i] = s[i];
    }
}

/* ─────────────────────────────────────────────────────────────
   SSE2 Hızlı Temizle — 4 piksel / iterasyon
   ─────────────────────────────────────────────────────────── */
static void GPU_Clear(uint32_t color) {
    /* color'ı 4'e çoğalt → 16-byte XMM */
    uint32_t total = SW * SH;
    uint32_t* p    = backbuffer;
    uint32_t  chunks = total / 4;

    __asm__ volatile (
        /* color'ı xmm0'a doldur */
        "movd    %1, %%xmm0\n"
        "pshufd  $0, %%xmm0, %%xmm0\n"
        "test    %2, %2\n"
        "jz      2f\n"
        "1:\n"
        "movdqu  %%xmm0, (%0)\n"
        "add     $16, %0\n"
        "dec     %2\n"
        "jnz     1b\n"
        "2:\n"
        : "+r"(p)
        : "r"(color), "r"(chunks)
        : "xmm0", "memory"
    );
    /* Kalan */
    uint32_t remain = total % 4;
    for (uint32_t i = 0; i < remain; i++) p[i] = color;
}

/* ─────────────────────────────────────────────────────────────
   Dirty Rect Tracker
   ─────────────────────────────────────────────────────────── */
struct DirtyRect {
    int x1, y1, x2, y2;
    bool dirty;

    DirtyRect() : x1(0),y1(0),x2(0),y2(0),dirty(false) {}

    void Mark(int x, int y, int w, int h) {
        if (!dirty) { x1=x; y1=y; x2=x+w; y2=y+h; dirty=true; return; }
        if (x   < x1) x1=x;
        if (y   < y1) y1=y;
        if (x+w > x2) x2=x+w;
        if (y+h > y2) y2=y+h;
    }

    void MarkAll() { x1=0; y1=0; x2=SW; y2=SH; dirty=true; }

    void Flush() {
        if (!dirty) return;
        GPU_BlitRect(x1, y1, x2-x1, y2-y1);
        dirty=false;
    }
};

/* ─────────────────────────────────────────────────────────────
   Gaussian Shadow Precompute
   ─────────────────────────────────────────────────────────── */

/* Yatay kutu-blur — tek satır */
static void BlurRow(uint8_t* row, uint8_t* tmp, int w, int r) {
    /* Sliding window toplamı */
    int sum = 0;
    int diam = 2*r+1;
    /* İlk pencereyi doldur */
    for (int i = -r; i <= r; i++) {
        int xi = i < 0 ? 0 : (i >= w ? w-1 : i);
        sum += row[xi];
    }
    for (int x = 0; x < w; x++) {
        tmp[x] = (uint8_t)(sum / diam);
        int add_x = x+r+1; if (add_x >= w) add_x = w-1;
        int rem_x = x-r;   if (rem_x <  0) rem_x = 0;
        sum += row[add_x] - row[rem_x];
    }
    for (int x = 0; x < w; x++) row[x] = tmp[x];
}

/* Dikey kutu-blur — tek sütun */
static void BlurCol(uint8_t* buf, uint8_t* tmp, int col, int w, int h, int r) {
    int sum = 0, diam = 2*r+1;
    for (int i = -r; i <= r; i++) {
        int yi = i < 0 ? 0 : (i >= h ? h-1 : i);
        sum += buf[yi*w+col];
    }
    for (int y = 0; y < h; y++) {
        tmp[y] = (uint8_t)(sum / diam);
        int add_y = y+r+1; if (add_y >= h) add_y = h-1;
        int rem_y = y-r;   if (rem_y <  0) rem_y = 0;
        sum += buf[add_y*w+col] - buf[rem_y*w+col];
    }
    for (int y = 0; y < h; y++) buf[y*w+col] = tmp[y];
}

/*
 * GPU_ComputeShadow — shadow_buf'a Gaussian gölge yaz
 * wx,wy,ww,wh : pencere koordinatları
 * ox,oy       : gölge ofseti (px)
 * blur_r      : blur yarıçapı (3 pass box-blur ≈ Gauss)
 * opacity     : maksimum alfa (0-255)
 */
static void GPU_ComputeShadow(int wx, int wy, int ww, int wh,
                               int ox, int oy,
                               int blur_r, uint8_t opacity)
{
    /* 1. shadow_buf'u sıfırla */
    int total = SW * (int)SH;
    for (int i = 0; i < total; i++) shadow_buf[i] = 0;

    /* 2. Gölge dikdörtgenini doldur */
    int sx = wx + ox, sy = wy + oy;
    int ex = sx + ww, ey = sy + wh;
    if (sx < 0) sx = 0; if (sy < 0) sy = 0;
    if (ex > (int)SW) ex = SW; if (ey > (int)SH-40) ey = SH-40;

    for (int y = sy; y < ey; y++)
        for (int x = sx; x < ex; x++)
            shadow_buf[y*SW+x] = opacity;

    /* 3. Separable Box-Blur × 3 (Gaussian yaklaşımı) */
    /* Satır tamponu: max 1024 byte yeterli */
    static uint8_t row_tmp[1024];
    static uint8_t col_tmp[768];

    for (int pass = 0; pass < 3; pass++) {
        /* Yatay */
        for (int y = 0; y < (int)SH; y++)
            BlurRow(shadow_buf + y*SW, row_tmp, SW, blur_r);
        /* Dikey */
        for (int x = 0; x < (int)SW; x++)
            BlurCol(shadow_buf, col_tmp, x, SW, SH, blur_r);
    }

    /* 4. Pencere alanının kendisini temizle (pencere üstüne çizilecek) */
    int cx = wx, cy = wy;
    int cex = cx+ww, cey = cy+wh;
    if (cx<0)cx=0; if(cy<0)cy=0;
    if(cex>(int)SW)cex=SW; if(cey>(int)SH)cey=SH;
    for (int y=cy; y<cey; y++)
        for (int x=cx; x<cex; x++)
            shadow_buf[y*SW+x] = 0;
}

/*
 * GPU_ApplyShadow — shadow_buf'daki alfa değerlerine göre
 * backbuffer piksellerini koyulaştır.
 * Sadece gölge bölgesini işler (bounding box).
 */
static void GPU_ApplyShadow(int wx, int wy, int ww, int wh,
                             int ox, int oy, int blur_r)
{
    int x1 = wx + ox - blur_r*3;
    int y1 = wy + oy - blur_r*3;
    int x2 = wx + ww + ox + blur_r*3;
    int y2 = wy + wh + oy + blur_r*3;

    if (x1 < 0) x1 = 0; if (y1 < 0) y1 = 0;
    if (x2 > (int)SW) x2 = SW;
    if (y2 > (int)(SH-40)) y2 = SH-40;

    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {
            uint8_t a = shadow_buf[y*SW+x];
            if (a == 0) continue;
            uint32_t& px = backbuffer[y*SW+x];
            int r = (px>>16)&0xFF;
            int g = (px>> 8)&0xFF;
            int b =  px     &0xFF;
            /* Alpha-blend: dst * (255-a) / 255 */
            r = r*(255-a)>>8;
            g = g*(255-a)>>8;
            b = b*(255-a)>>8;
            px = ((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
        }
    }
}

#endif
