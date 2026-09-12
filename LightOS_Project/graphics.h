#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "shared.h"
#include "font.h"

extern uint32_t* backbuffer;

/* ────────────────────────────────────────
   Temel piksel yazma
   ──────────────────────────────────────── */
static inline void PutPixel(int x, int y, uint32_t color) {
    if ((unsigned)x < SW && (unsigned)y < SH)
        backbuffer[y * SW + x] = color;
}

/* ────────────────────────────────────────
   Renk aritmetiği
   ──────────────────────────────────────── */
static inline uint8_t ClampU8(int v) {
    return v < 0 ? 0 : (v > 255 ? 255 : (uint8_t)v);
}

/* Doğrusal interpolasyon */
static inline uint32_t LerpColor(uint32_t a, uint32_t b, int t, int max) {
    if (max <= 0) return a;
    int ar = (a>>16)&0xFF, ag = (a>>8)&0xFF, ab = a&0xFF;
    int br = (b>>16)&0xFF, bg = (b>>8)&0xFF, bb = b&0xFF;
    return ((uint32_t)ClampU8(ar+(br-ar)*t/max) << 16) |
           ((uint32_t)ClampU8(ag+(bg-ag)*t/max) <<  8) |
           (uint32_t)ClampU8(ab+(bb-ab)*t/max);
}

/* Rengi koyulaştır / açar */
static inline uint32_t DimColor(uint32_t c, int pct) {  /* pct 0-100 */
    return LerpColor(0x000000, c, pct, 100);
}
static inline uint32_t BrightColor(uint32_t c, int pct) { /* pct 100-200 */
    return LerpColor(c, 0xFFFFFF, pct-100, 100);
}

/* ────────────────────────────────────────
   Çizim fonksiyonları
   ──────────────────────────────────────── */
static void DrawRect(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < h; i++)
        for (int j = 0; j < w; j++)
            PutPixel(x+j, y+i, color);
}

/* Yatay gradyan */
static void DrawGradientV(int x, int y, int w, int h, uint32_t top, uint32_t bot) {
    for (int i = 0; i < h; i++)
        DrawRect(x, y+i, w, 1, LerpColor(top, bot, i, h > 1 ? h-1 : 1));
}

/* Yatay gradyan (soldan sağa) */
static void DrawGradientH(int x, int y, int w, int h, uint32_t left, uint32_t right) {
    for (int j = 0; j < w; j++)
        DrawRect(x+j, y, 1, h, LerpColor(left, right, j, w > 1 ? w-1 : 1));
}

/* Köşeli kenarlık */
static void DrawRectBorder(int x, int y, int w, int h, uint32_t color) {
    DrawRect(x,     y,     w,  1, color);
    DrawRect(x,     y+h-1, w,  1, color);
    DrawRect(x,     y,     1,  h, color);
    DrawRect(x+w-1, y,     1,  h, color);
}

/* XP-style 3-D kenarlık: üst-sol açık, alt-sağ koyu */
static void DrawBevel(int x, int y, int w, int h, bool raised) {
    uint32_t hi = 0xFFFFFF, lo = 0x808080;
    if (!raised) { hi = 0x808080; lo = 0xFFFFFF; }
    DrawRect(x,     y,     w,  1, hi); /* üst */
    DrawRect(x,     y,     1,  h, hi); /* sol */
    DrawRect(x,     y+h-1, w,  1, lo); /* alt */
    DrawRect(x+w-1, y,     1,  h, lo); /* sağ */
    DrawRect(x+1,   y+1,   w-2,1, BrightColor(hi, 115)); /* iç üst */
    DrawRect(x+1,   y+1,   1, h-2,BrightColor(hi, 115)); /* iç sol */
}

/*
 * DrawRealShadow — Gerçek piksel karıştırmalı gölge
 *
 * Backbuffer'daki pikselleri okur ve mesafeye göre koyulaştırır.
 * - OX/OY : gölge ofseti
 * - PASSES : bulanıklık katmanı (her pass biraz daha genişler, biraz daha açık)
 * Pencere alanının altındaki pikseller atlanır (pencere zaten üstüne çizilecek).
 */
static void DrawRealShadow(int wx, int wy, int ww, int wh) {
    const int OX    = 8;   /* sağa ofset */
    const int OY    = 9;   /* aşağı ofset */
    const int PASSES= 10;  /* blur katman sayısı */

    for (int pass = PASSES; pass >= 1; pass--) {
        int expand = (PASSES - pass) / 2;      /* her adım biraz büyür */
        int sx = wx + OX - expand;
        int sy = wy + OY - expand;
        int sw = ww + expand * 2;
        int sh = wh + expand * 2;

        /* En dışta en açık, içe doğru daha koyu */
        int darken = pass * 5;   /* 5 .. 50 */
        if (darken > 52) darken = 52;

        for (int py = sy; py < sy + sh; py++) {
            if (py < 0 || py >= (int)(SH - 40)) continue;
            for (int px = sx; px < sx + sw; px++) {
                if (px < 0 || px >= (int)SW) continue;
                /* Pencere kendi piksellerini zaten çizecek, atla */
                if (px >= wx && px < wx+ww && py >= wy && py < wy+wh) continue;

                uint32_t& p = backbuffer[py * SW + px];
                int r = (p >> 16) & 0xFF;
                int g = (p >>  8) & 0xFF;
                int b =  p        & 0xFF;
                r = r * (100 - darken) / 100;
                g = g * (100 - darken) / 100;
                b = b * (100 - darken) / 100;
                p = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
    }
}

/* Eski isim — geriye dönük uyum için sarmalayıcı */
static void DrawShadow(int x, int y, int w, int h) {
    DrawRealShadow(x, y, w, h);
}

/* Noktalı doku (masaüstü için) */
static void DrawDotPattern(int x, int y, int w, int h, uint32_t dot, int step) {
    for (int j = 0; j < h; j += step)
        for (int i = 0; i < w; i += step)
            PutPixel(x+i, y+j, dot);
}

/* ────────────────────────────────────────
   UTF-8 çözümleyici
   ──────────────────────────────────────── */
static unsigned int utf8_next(const char** s) {
    unsigned char c = (unsigned char)**s;
    if (!c) return 0;
    (*s)++;
    if (c < 0x80) return c;
    unsigned int cp;
    unsigned char c2 = (unsigned char)**s; (*s)++;
    if ((c & 0xE0) == 0xC0) {
        cp = ((unsigned int)(c & 0x1F) << 6) | (c2 & 0x3F);
        return cp;
    }
    /* 3-byte */
    unsigned char c3 = (unsigned char)**s; (*s)++;
    cp = ((unsigned int)(c & 0x0F) << 12) |
         ((unsigned int)(c2 & 0x3F) << 6) |
         (c3 & 0x3F);
    return cp;
}

/* ────────────────────────────────────────
   Karakter çizimi (UTF-8 codepoint)
   ──────────────────────────────────────── */
static void DrawCharCP(int x, int y, unsigned int cp, uint32_t color) {
    const unsigned char* glyph = 0;
    if (cp >= 0x20 && cp < 0x80) {
        glyph = font_ascii[cp - 0x20];
    } else {
        int ti = ext_glyph_id(cp);
        if (ti >= 0) glyph = font_ext[ti];
        else glyph = font_ext[FONT_PLACEHOLDER_ID]; /* □ — desteklenmeyen
            karakter artık sessizce kaybolmuyor, fark edilir bir kutu
            gösteriliyor (ör. henüz eklenmemiş bir sembol/emoji). */
    }
    if (!glyph) return;
    for (int row = 0; row < 8; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < 8; col++)
            if (bits & (0x80 >> col))
                PutPixel(x+col, y+row, color);
    }
}

/* Eski DrawChar uyumu */
static void DrawChar(int x, int y, char c, uint32_t color) {
    DrawCharCP(x, y, (unsigned char)c, color);
}

/* ─── DrawString: UTF-8 parse eder ─── */
static void DrawString(int x, int y, const char* str, uint32_t color) {
    if (!str) return;
    int ox = x;
    while (*str) {
        unsigned int cp = utf8_next(&str);
        if (cp == 0) break;
        if (cp == '\n') { y += 11; x = ox; }
        else if (cp == '\t') { x += 28; }
        else { DrawCharCP(x, y, cp, color); x += 9; }
    }
}

/* Gölgeli yazı (okunabilirlik için) */
static void DrawStringShadow(int x, int y, const char* str, uint32_t color) {
    DrawString(x+1, y+1, str, 0x202020);
    DrawString(x,   y,   str, color);
}

/* Verilen arkaplan rengine göre okunabilir metin rengi (siyah/beyaz) döner.
 * Basit göreli parlaklık (luminance) hesabı: açık zeminde siyah,
 * koyu zeminde beyaz metin daha okunaklı olur. */
static inline uint32_t ContrastTextColor(uint32_t bg){
    int r=(bg>>16)&0xFF, g=(bg>>8)&0xFF, b=bg&0xFF;
    int luma=(r*299+g*587+b*114)/1000; /* 0-255, ITU-R BT.601 ağırlıkları */
    return luma>140 ? 0x000000 : 0xFFFFFF;
}

/* Metni okunabilir kılmak için, arkaplanın parlaklığına göre siyah/beyaz
 * seçilmiş renkte ve karşıt renkte ince bir gölgeyle çizer — sabit bir
 * kutu/panel çizmeden (siyah dikdörtgen arkaplan yerine) her zemin
 * üzerinde okunaklı kalır. bgSample: metnin üzerine bineceği zeminden
 * alınmış temsili bir renk (ör. o bölgeyi domine eden renk). */
static void DrawStringAdaptive(int x, int y, const char* str, uint32_t bgSample) {
    uint32_t tc = ContrastTextColor(bgSample);
    uint32_t shadow = (tc==0x000000) ? 0xFFFFFF : 0x000000;
    DrawString(x+1, y+1, str, shadow);
    DrawString(x,   y,   str, tc);
}

/* Ortalı yazı */
static int StringWidth(const char* str) {
    if (!str) return 0;
    int w = 0;
    const char* p = str;
    while (*p) { utf8_next(&p); w += 9; }
    return w > 0 ? w - 1 : 0;
}

static void DrawStringCentered(int cx, int y, const char* str, uint32_t color) {
    int w = StringWidth(str);
    DrawString(cx - w/2, y, str, color);
}

/* ────────────────────────────────────────
   Alias (eski kodla uyum)
   ──────────────────────────────────────── */
#define DrawGradient DrawGradientV

#endif
