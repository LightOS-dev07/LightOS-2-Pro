#ifndef NOTEPAD_H
#define NOTEPAD_H
#include "../gui.h"
#include "../kernel/vfs.h"

class Notepad : public Window {
public:
    static const int BUF_SIZE = 4096;
    unsigned int codepoints[BUF_SIZE];
    int  cursor_pos;
    int  scroll_y;      /* kaydırma (satır) */
    int  vfs_node;      /* hangi VFS dosyası düzenleniyor, -1=yeni */
    bool modified;      /* kaydetilmemiş değişiklik var mı */
    char filename[32];  /* gösterim için */

    Notepad();
    void DrawContent() override;
    void OnClickContent(int mx, int my) override;
    void KeyPressCP(unsigned int cp);
    void UpdateTitle(const char* t);
    void OpenVfsNode(int idx);
    void SaveToVfs();
private:
    int CountLines();
    void GetLineCol(int& line, int& col);
};
#endif
