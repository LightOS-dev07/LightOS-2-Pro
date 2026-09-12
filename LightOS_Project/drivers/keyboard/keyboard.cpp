#include "keyboard.h"

int g_kbd_layout = 0; /* 0=EN, 1=TR */

/* ── EN (US) Scancode Set 1 ── */
static const char en_map[] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`', 0,
    '\\','z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' '
};
static const char en_shift[] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"', '~', 0,
    '|', 'Z','X','C','V','B','N','M','<','>','?', 0, '*', 0, ' '
};

/*
 * TR-Q Scancode Set 1
 * Harflerin büyüğü/küçüğü UTF-8 single-byte değil, codepoint ile çözülür.
 * Burada ASCII sığmayan karakterler için 0x01..0x0C arası özel kod kullanıyoruz:
 *   0x01=ğ  0x02=ü  0x03=ş  0x04=ı  0x05=ö  0x06=ç
 *   0x11=Ğ  0x12=Ü  0x13=Ş  0x14=İ  0x15=Ö  0x16=Ç
 */
static const unsigned char tr_map[] = {
/*00*/  0,   27,  '1','2','3','4','5','6','7','8','9','0',
/*0C*/  '*', '-', '\b',
/*0F*/  '\t',
/*10*/  'q','w','e','r','t','y','u',
/*17*/  0x04,  /* ı (noktasız i) */
/*18*/  'o','p',
/*1A*/  0x01,  /* ğ */
/*1B*/  0x02,  /* ü */
/*1C*/  '\n',
/*1D*/  0,
/*1E*/  'a','s','d','f','g','h','j','k','l',
/*27*/  0x03,  /* ş */
/*28*/  'i',   /* i (noktalı) */
/*29*/  '"',
/*2A*/  0,
/*2B*/  ',',
/*2C*/  'z','x','c','v','b','n','m',
/*33*/  0x05,  /* ö */
/*34*/  0x06,  /* ç */
/*35*/  '.',
/*36*/  0, '*', 0, ' '
};
static const unsigned char tr_shift[] = {
/*00*/  0,  27, '!','\'','#','+','%','&','/',
/*09*/  '(',')','=','?','_','\b',
/*0F*/  '\t',
/*10*/  'Q','W','E','R','T','Y','U',
/*17*/  0x14, /* İ */
/*18*/  'O','P',
/*1A*/  0x11, /* Ğ */
/*1B*/  0x12, /* Ü */
/*1C*/  '\n',
/*1D*/  0,
/*1E*/  'A','S','D','F','G','H','J','K','L',
/*27*/  0x13, /* Ş */
/*28*/  'I',
/*29*/  '(',
/*2A*/  0,
/*2B*/  ';',
/*2C*/  'Z','X','C','V','B','N','M',
/*33*/  0x15, /* Ö */
/*34*/  0x16, /* Ç */
/*35*/  ':',
/*36*/  0, '*', 0, ' '
};

/* Özel kod → Unicode codepoint */
static unsigned int tr_special_cp(unsigned char code) {
    switch(code) {
        case 0x01: return 0x011F; /* ğ */
        case 0x02: return 0x00FC; /* ü */
        case 0x03: return 0x015F; /* ş */
        case 0x04: return 0x0131; /* ı */
        case 0x05: return 0x00F6; /* ö */
        case 0x06: return 0x00E7; /* ç */
        case 0x11: return 0x011E; /* Ğ */
        case 0x12: return 0x00DC; /* Ü */
        case 0x13: return 0x015E; /* Ş */
        case 0x14: return 0x0130; /* İ */
        case 0x15: return 0x00D6; /* Ö */
        case 0x16: return 0x00C7; /* Ç */
        default:   return 0;
    }
}

static const int MAP_SIZE = (int)sizeof(tr_map);

Keyboard::Keyboard() : shift_pressed(false), caps_lock(false) {}

/*
 * UpdateCP: Codepoint döndürür (TR özel + caps_lock mantığı dahil)
 * 0 = karakter yok (modifier vb.)
 */
unsigned int Keyboard::UpdateCP(uint8_t sc) {
    /* Bırakma olayı */
    if (sc & 0x80) {
        uint8_t r = sc & 0x7F;
        if (r==0x2A||r==0x36) shift_pressed=false;
        return 0;
    }
    switch(sc) {
        case 0x2A: case 0x36: shift_pressed=true; return 0;
        case 0x3A: caps_lock=!caps_lock;           return 0;
        case 0x1C: return '\n';
        case 0x0E: return '\b';
        case 0x39: return ' ';
    }

    if (g_kbd_layout == 0) {
        /* EN */
        if (sc >= sizeof(en_map)) return 0;
        char c = shift_pressed ? en_shift[sc] : en_map[sc];
        if (!c) return 0;
        if (caps_lock && !shift_pressed && c>='a'&&c<='z') c-=32;
        else if (caps_lock &&  shift_pressed && c>='A'&&c<='Z') c+=32;
        return (unsigned int)(unsigned char)c;
    } else {
        /* TR */
        if (sc >= MAP_SIZE) return 0;
        unsigned char c = shift_pressed ? tr_shift[sc] : tr_map[sc];
        if (!c) return 0;
        /* Özel TR kodu mu? */
        if (c < 0x20) {
            unsigned int cp = tr_special_cp(c);
            /* caps_lock mantığı: büyük/küçük dönüşümü shift ile ters */
            if (caps_lock && !shift_pressed && cp>=0x61&&cp<=0x7A) {
                /* ASCII küçük → büyük */
                unsigned char up = shift_pressed ? tr_shift[sc] : tr_map[sc];
                return tr_special_cp(up ^ 0x10); /* 0x01^0x10=0x11 vb. */
            }
            return cp;
        }
        if (caps_lock && !shift_pressed && c>='a'&&c<='z') c-=32;
        else if (caps_lock &&  shift_pressed && c>='A'&&c<='Z') c+=32;
        return (unsigned int)(unsigned char)c;
    }
}

/* Geriye dönük uyum: sadece ASCII döndürür */
char Keyboard::Update(uint8_t sc) {
    unsigned int cp = UpdateCP(sc);
    if (cp < 0x80) return (char)cp;
    return 0; /* TR özel → shell'de UpdateCP ile ele alınır */
}
