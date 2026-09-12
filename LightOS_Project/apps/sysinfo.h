#ifndef SYSINFO_H
#define SYSINFO_H
#include "../gui.h"
class SysInfo : public Window {
public:
    SysInfo();
    void DrawContent() override;
    void UpdateLang();
private:
    void DrawBar(int x,int y,int w,int h,int pct,uint32_t col);
};
#endif
