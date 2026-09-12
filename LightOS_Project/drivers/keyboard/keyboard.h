#ifndef KEYBOARD_H
#define KEYBOARD_H
#include <stdint.h>

/* Global klavye düzeni: 0=EN, 1=TR */
extern int g_kbd_layout;

class Keyboard {
public:
    Keyboard();
    char Update(uint8_t scancode);
    /* TR özel karakterler için genişletilmiş: codepoint döndürür */
    unsigned int UpdateCP(uint8_t scancode);
private:
    bool shift_pressed;
    bool caps_lock;
};
#endif
