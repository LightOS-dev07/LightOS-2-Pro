#ifndef FILEMGR_H
#define FILEMGR_H
#include "../gui.h"
#include "../kernel/vfs.h"

class FileMgr : public Window {
public:
    int  cwd;           /* current working directory (VFS node idx) */
    int  selected;      /* seçili child indeksi (0-based), -1=yok */
    int  scroll;        /* kaydırma ofseti */
    bool rename_mode;
    int  open_request_node;  /* -1 ya da notepadde açılacak VFS node */   /* ad değiştirme aktif mi */
    char rename_buf[VFS_NAME_LEN];
    int  rename_cursor;

    FileMgr();
    void DrawContent()  override;
    void OnClickContent(int mx, int my) override;
    void UpdateLang();
    void KeyPress(char c);

    /* Toolbar butonları */
    void DoNewFolder();
    void DoNewFile();
    void DoDelete();
    void DoBack();

private:
    void DrawSidebar();
    void DrawToolbar();
    void DrawFileList();
    void DrawStatusBar();
    void DrawFileIcon(int fx, int fy, bool is_dir, bool sel, bool hov);
    void DrawBreadcrumb();
    void BuildPath(int node_idx, char* buf, int buf_size);
    int  GetChild(int nth); /* nth görünür child'ı döndür */
};
#endif
