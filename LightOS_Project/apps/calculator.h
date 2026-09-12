#ifndef CALCULATOR_H
#define CALCULATOR_H
#include "../gui.h"

class Calculator : public Window {
public:
    int  current_val, accumulator;
    char last_op;
    bool new_input, error;
    char display[16];

    Calculator();
    void DrawContent()  override;
    void OnClickContent(int mx, int my) override;
    void UpdateLang();

private:
    void DrawButton(int bx,int by,int bw,int bh,
                    const char* label,uint32_t bg,uint32_t fg);
    void PressButton(int row,int col);
    void UpdateDisplay();
    static void itoa_s(int v,char* buf);
};
#endif
