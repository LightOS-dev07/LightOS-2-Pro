#ifndef CLOCK_APP_H
#define CLOCK_APP_H
#include "../gui.h"
class ClockApp : public Window {
public:
    ClockApp();
    void DrawContent() override;
private:
    void DrawAnalog(int cx, int cy, int r);
};
#endif
