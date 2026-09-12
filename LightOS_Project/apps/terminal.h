#ifndef TERMINAL_H
#define TERMINAL_H
#include "../gui.h"
#include "../kernel/vfs.h"

class Terminal : public Window {
public:
    static const int COLS    = 60;
    static const int ROWS    = 18;
    static const int HISTORY = 32;

    char   output[ROWS][COLS+1];   /* ekran tamponu */
    int    out_rows;               /* dolu satır sayısı */
    char   input[128];             /* mevcut satır */
    int    input_len;
    char   history[HISTORY][128];  /* komut geçmişi */
    int    hist_count;
    int    hist_idx;
    int    cwd_node;               /* terminal'in şu anki dizini */
    uint32_t text_color;            /* çıktı metni rengi (color komutu) */

    Terminal();
    void DrawContent()  override;
    void OnClickContent(int mx, int my) override;
    void KeyPress(unsigned int cp);
    void UpdateLang();

private:
    void PrintLine(const char* text);
    void PrintChar(char c);
    void RunCommand(const char* cmd);
    void ScrollUp();
    /* Komutlar */
    void CmdHelp();
    void CmdLs(const char* arg);
    void CmdCd(const char* path);
    void CmdCat(const char* name);
    void CmdEcho(const char* msg);
    void CmdClear();
    void CmdMkdir(const char* name);
    void CmdRm(const char* name);
    void CmdPwd();
    void CmdVer();
    void CmdMem();
    void CmdTouch(const char* name);
    void CmdWrite(const char* args);
    void CmdDate();
    void CmdWhoami();
    void CmdUptime();
    void CmdHistory();
    void CmdSysinfo();
    void CmdCalc(const char* expr);
    void CmdColor(const char* arg);
    /* Yardımcılar */
    static int t_strcmp(const char* a, const char* b);
    static int t_strlen(const char* s);
    static void t_strcpy(char* d, const char* s);
    static const char* t_skip(const char* s); /* ilk boşluktan sonrasını döndür */
};
#endif
