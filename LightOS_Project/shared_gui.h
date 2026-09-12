#ifndef SHARED_GUI_H
#define SHARED_GUI_H

#include "graphics.h"
#include "drivers/keyboard/keyboard.h"

// --- YARDIMCI FONKSİYONLAR ---
int strlen(const char* str) { int i=0; while(str[i]) i++; return i; }
void strcpy(char* dest, const char* src) { int i=0; while(src[i]){ dest[i]=src[i]; i++; } dest[i]=0; }
int strcmp(const char* s1, const char* s2) {
    while(*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// --- PENCERE SİSTEMİ (BASE) ---
struct Window {
    int x, y, w, h;
    bool visible, minimized, dragging;
    char title[32];
    int drag_off_x, drag_off_y;

    Window(const char* t, int _x, int _y, int _w, int _h) {
        strcpy(title, t);
        x = _x; y = _y; w = _w; h = _h;
        visible = false; minimized = false; dragging = false;
    }

    virtual void Draw() {
        if (!visible || minimized) return;
        // Gölge
        DrawRect(x+5, y+5, w, h, 0x101010);
        // Gövde
        DrawRect(x, y, w, h, 0xE0E0E0);
        // Başlık
        DrawRect(x, y, w, 25, 0x000080); // Lacivert Başlık
        // Butonlar
        DrawRect(x+w-20, y+5, 15, 15, 0xFF0000); // Kapat [X]
        DrawRect(x+w-40, y+5, 15, 15, 0xFFFF00); // Küçült [-]
        
        DrawContent();
    }

    virtual void DrawContent() {} // Override edilecek

    virtual bool HandleClick(int mx, int my, bool click_state) {
        if (!visible || minimized) return false;

        // Başlık çubuğu (Sürükleme ve Butonlar)
        if (mx >= x && mx <= x+w && my >= y && my <= y+25) {
            if (mx >= x+w-20) { visible = false; return true; } // Kapat
            if (mx >= x+w-40 && mx < x+w-20) { minimized = true; return true; } // Küçült
            
            if (click_state) { dragging = true; drag_off_x = mx - x; drag_off_y = my - y; }
            return true;
        }
        
        // İçerik Tıklaması
        if (mx >= x && mx <= x+w && my > y+25 && my <= y+h) {
            OnClickContent(mx, my);
            return true;
        }
        return false;
    }

    virtual void OnClickContent(int mx, int my) {}
    
    void UpdateDrag(int mx, int my, bool hold) {
        if (dragging) {
            if (hold) { x = mx - drag_off_x; y = my - drag_off_y; }
            else dragging = false;
        }
    }
};

// --- 1. NOTEPAD ---
class Notepad : public Window {
public:
    char buffer[512];
    int cursor;
    Notepad() : Window("Notepad.exe", 100, 100, 300, 200) { cursor=0; buffer[0]=0; }
    
    void DrawContent() override {
        DrawRect(x+5, y+30, w-10, h-40, 0xFFFFFF); // Beyaz Kağıt
        // Basit font simülasyonu (Çizgi çizgi gösterir)
        int cx = x+10, cy = y+35;
        for(int i=0; i<cursor; i++) {
            DrawRect(cx, cy, 5, 8, 0x000000); // Harf yerine kutu
            cx += 7;
            if(cx > x+w-20) { cx = x+10; cy += 12; }
        }
        DrawRect(cx, cy, 2, 10, 0xFF0000); // İmleç
    }
    
    void KeyPress(char c) {
        if(c == '\b') { if(cursor>0) buffer[--cursor]=0; }
        else if(cursor<511) { buffer[cursor++]=c; buffer[cursor]=0; }
    }
};

// --- 2. HESAP MAKİNESİ ---
class Calculator : public Window {
public:
    int result;
    Calculator() : Window("Calc", 150, 150, 200, 250) { result = 0; }

    void DrawContent() override {
        DrawRect(x+10, y+35, w-20, 30, 0xFFFFFF); // Ekran
        // Tuşlar (Grid)
        for(int i=0; i<4; i++) for(int j=0; j<3; j++) {
            DrawRect(x+10 + (j*50), y+80 + (i*40), 40, 30, 0xAAAAAA);
        }
    }
};

// --- 3. DOSYA YÖNETİCİSİ (FILEMGR) ---
class FileMgr : public Window {
public:
    char path[64];
    FileMgr() : Window("File Explorer", 200, 200, 400, 300) { strcpy(path, "C:\\"); }

    void DrawContent() override {
        // Adres Çubuğu
        DrawRect(x+5, y+30, w-10, 20, 0xFFFFFF);
        
        // Klasör İkonları
        DrawFolder(x+20, y+60, "SYSTEM");
        DrawFolder(x+100, y+60, "USERS");
        DrawFolder(x+180, y+60, "DOCS");
        
        // Kenar Çubuğu
        DrawRect(x, y+25, 80, h-25, 0xCCCCCC);
        DrawRect(x+5, y+40, 70, 20, 0x000080); // Disk C
    }

    void DrawFolder(int dx, int dy, const char* name) {
        DrawRect(dx, dy, 40, 30, 0xFFCC00); // Sarı Klasör
        DrawRect(dx, dy-5, 15, 5, 0xFFCC00); // Kulakçık
    }
};

// Tek bir pikseli boyayan hızlı fonksiyon
void PutPixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < SW && y >= 0 && y < SH) {
        backbuffer[y * SW + x] = color;
    }
}

#endif