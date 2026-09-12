#ifndef THEME_H
#define THEME_H
#include <stdint.h>

enum ThemeID { THEME_LUNA=0, THEME_ICE=1, THEME_AMBER=2 };
extern ThemeID g_theme;

struct ThemePalette {
    uint32_t title_foc_top,title_foc_bot,title_foc_shine,title_foc_text;
    uint32_t title_unf_top,title_unf_bot,title_unf_text;
    uint32_t win_frame_foc,win_frame_unf;
    uint32_t win_body,win_content;
    uint32_t btn_close_top,btn_close_bot;
    uint32_t btn_min_top,btn_min_bot;
    uint32_t taskbar_top,taskbar_bot,taskbar_line;
    uint32_t start_top,start_bot,start_hover_top,start_hover_bot;
    uint32_t desk_top,desk_bot;
    bool     ice_glass; /* başlık çubuğunda cam efekti */
};

static const ThemePalette THEMES[3] = {
/* Luna Steel */
{
    0x0A246A,0x2468B0,0x4A90D8,0xFFFFFF,
    0x6A7888,0x8A98A8,0xDDDDDD,
    0x0054E3,0x7A8898,
    0xD4D0C8,0xF5F5F0,
    0xFF6060,0xBB1010,
    0xFFCC44,0xCC8800,
    0x1A4080,0x0C2040,0x5090C8,
    0x207830,0x104820,0x309A40,0x207830,
    0x1A5C9A,0x0C3060,
    false
},
/* ColdIceBar */
{
    0xC0D8F0,0xE0F0FF,0xFFFFFF,0x1A3A5C,
    0xD8E8F4,0xECF4FC,0x7090A8,
    0x88B8D8,0xB0C8D8,
    0xECF4FC,0xF8FBFF,
    0xFF9090,0xCC3030,
    0xFFE080,0xCCAA00,
    0xC8E0F0,0xA0C4E0,0xFFFFFF,
    0x7AB8DC,0x5090B8,0x90CCEC,0x6EB0DC,
    0xC0DCF0,0xE0F0FF,
    true
},
/* Amber Dark */
{
    0xFF9248,0xCC6A28,0xFFB878,0x1A0800,
    0x8A5A00,0x5A3A00,0x202020,
    0xCC6A28,0x6A3A10,
    0x1A0F00,0xFDF6EA,
    0xCC4400,0x882200,
    0x8A5A00,0x4A3000,
    0x1A0F00,0x0D0700,0x4A2800,
    0xFF9248,0xCC6A28,0xFFB878,0xFF9248,
    0x0D0700,0x080400,
    false
},
};

static inline const ThemePalette& TP(){ return THEMES[(int)g_theme]; }
#endif

/* ── Amber Dark Tema ── */
/* Sıcak turuncu-amber, koyu arka plan */
