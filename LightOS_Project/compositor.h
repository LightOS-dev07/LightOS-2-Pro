#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "gui.h"

static const int COMP_MAX = 48;

struct Compositor {
    Window*   stack[COMP_MAX];
    int       count;

    static const int SHADOW_OX   = 8;
    static const int SHADOW_OY   = 10;
    static const int SHADOW_BLUR = 7;

    Compositor() : count(0) {}

    void Add(Window* w) {
        if (count < COMP_MAX) stack[count++] = w;
    }

    void BringToFront(int idx) {
        if (idx < 0 || idx >= count || idx == count-1) return;
        Window* w = stack[idx];
        for (int i = idx; i < count-1; i++) stack[i] = stack[i+1];
        stack[count-1] = w;
    }

    int HitTest(int mx, int my) {
        for (int i = count-1; i >= 0; i--) {
            Window* w = stack[i];
            if (!w->visible || w->minimized) continue;
            if (mx>=w->x && mx<=w->x+w->w && my>=w->y && my<=w->y+w->h)
                return i;
        }
        return -1;
    }

    /*
     * Gaussian-approx shadow:
     * Birden fazla pass ile backbuffer piksellerini koyulaştır.
     * Her pass biraz daha geniş, biraz daha açık → yumuşak blur hissi.
     */
    void DrawShadowFor(Window* w) {
        if (w->anim_frame < Window::ANIM_MAX) return;

        const int OX = SHADOW_OX, OY = SHADOW_OY;
        const int PASSES = SHADOW_BLUR;

        for (int pass = PASSES; pass >= 1; pass--) {
            int expand = (PASSES - pass);
            int sx = w->x + OX - expand;
            int sy = w->y + OY - expand;
            int sw = w->w + expand * 2;
            int sh = w->h + expand * 2;

            /* Koyulaştırma oranı: en dışta az, içe doğru fazla */
            int darken = pass * 6;
            if (darken > 55) darken = 55;

            for (int py = sy; py < sy + sh; py++) {
                if (py < 0 || py >= (int)(SH - 40)) continue;
                for (int px = sx; px < sx + sw; px++) {
                    if (px < 0 || px >= (int)SW) continue;
                    /* Pencere kendi alanında: atla (pencere üstüne çizilecek) */
                    if (px >= w->x && px < w->x+w->w &&
                        py >= w->y && py < w->y+w->h) continue;
                    uint32_t& p = backbuffer[py * SW + px];
                    int r = (int)((p >> 16) & 0xFF) * (100 - darken) / 100;
                    int g = (int)((p >>  8) & 0xFF) * (100 - darken) / 100;
                    int b = (int)( p        & 0xFF) * (100 - darken) / 100;
                    p = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
                }
            }
        }
    }

    void Draw() {
        for (int i = 0; i < count; i++) {
            Window* w = stack[i];
            if (!w->visible || w->minimized) continue;
            bool focused = (i == count - 1);
            DrawShadowFor(w);
            w->Draw(focused);
        }
    }

    bool HandleClick(int mx, int my, bool press) {
        int hit = HitTest(mx, my);
        if (hit < 0) return false;
        if (press && hit != count-1) BringToFront(hit);
        return stack[count-1]->HandleClick(mx, my, press);
    }

    void UpdateDrag(int mx, int my, bool held) {
        for (int i = 0; i < count; i++)
            stack[i]->UpdateDrag(mx, my, held);
    }

    Window* Get(int i) {
        return (i >= 0 && i < count) ? stack[i] : 0;
    }
};

#endif
