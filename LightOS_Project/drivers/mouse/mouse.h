#ifndef MOUSE_H
#define MOUSE_H
#include <stdint.h>

static const int SENS_MAX = 14;

class Mouse {
public:
    int     x, y;
    bool    left, right, middle, has_moved;
    uint8_t cycle, packet[3];
    Mouse();
    void Feed(uint8_t data);
};
#endif
