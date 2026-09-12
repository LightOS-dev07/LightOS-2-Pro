#include "graphics.h"
#include "drivers/mouse/mouse.h"
#include "drivers/keyboard/keyboard.h"

extern uint32_t* backbuffer;
extern "C" void outb(unsigned short port, unsigned char val);
extern "C" unsigned char inb(unsigned short port);

// BMP Yükleyemediğimiz için kodla ikon çiziyoruz
void DrawUserIcon(int x, int y) {
    DrawRect(x, y, 60, 60, 0xFFFFFF); // Çerçeve
    DrawRect(x+15, y+10, 30, 30, 0x555555); // Kafa
    DrawRect(x+5, y+45, 50, 15, 0x555555); // Gövde
}

bool LogonLoop() {
    Mouse mouse;
    // Arkaplan (Mavi Windows stili)
    DrawRect(0, 0, SW, SH, 0x0099CC);

    // Login Kutusu
    int boxX = SW/2 - 150;
    int boxY = SH/2 - 100;
    DrawRect(boxX, boxY, 300, 200, 0x000000); // Gölge
    DrawRect(boxX-2, boxY-2, 300, 200, 0xFFFFFF); // Kutu

    DrawUserIcon(SW/2 - 30, boxY + 20);

    // Giriş Butonu
    int btnX = SW/2 - 50;
    int btnY = boxY + 140;
    DrawRect(btnX, btnY, 100, 30, 0x00AA00); // Yeşil Buton

    bool running = true;
    while(running) {
        // --- INPUT ---
        uint8_t status = inb(0x64);
        if((status & 1) && (status & 0x20)) { // Mouse
            mouse.Feed(inb(0x60));
        }

        if(mouse.has_moved || mouse.left) {
            // İmleci Çiz
            // (Burada normalde Double Buffer swap lazım ama basit tutuyoruz)
            DrawRect(mouse.x, mouse.y, 5, 5, 0x000000);
            
            // Butona tıklandı mı?
            if(mouse.left && mouse.x >= btnX && mouse.x <= btnX+100 && mouse.y >= btnY && mouse.y <= btnY+30) {
                return true; // GİRİŞ BAŞARILI
            }
            mouse.has_moved = false;
        }
    }
    return false;
}