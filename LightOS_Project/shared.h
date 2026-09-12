#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>

/* Ekran çözünürlüğü — runtime'da screen_w/screen_h ile override edilir
   ama sabit array boyutları ve PutPixel bounds için bu değerler kullanılır */
#define SW 1024
#define SH 768

/* Renkler */
#define COL_BG      0x202020
#define COL_WIN_BG  0xFFFFFF
#define COL_TITLE   0x303030
#define COL_TEXT    0x000000
#define COL_RED     0xFF5555
#define COL_GREEN   0x55FF55
#define COL_YELLOW  0xFFAA00

#endif
