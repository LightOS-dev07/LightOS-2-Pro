#ifndef SETTINGS_H
#define SETTINGS_H
#include "../gui.h"
#include "../lang.h"
#include "../theme.h"

class Settings : public Window {
public:
    int  active_tab;      /* 0=Keyboard, 1=Theme, 2=Wallpaper */
    int  kbd_selection;
    int  theme_selection;
    int  wallpaper_selection;
    bool apply_pressed;

    Settings();
    void DrawContent()  override;
    void OnClickContent(int mx,int my) override;
    void UpdateLang();
private:
    void DrawTab(int tx,int ty,int tw,int th,const char* lbl,bool active);
    void DrawKeyboardTab();
    void DrawWallpaperTab();
    void DrawThemeTab();
};
#endif
