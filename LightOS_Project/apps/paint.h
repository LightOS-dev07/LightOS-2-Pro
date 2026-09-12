#ifndef PAINT_H
#define PAINT_H
#include "../gui.h"
class Paint : public Window {
public:
    static const int CANVAS_W = 320;
    static const int CANVAS_H = 200;
    uint32_t canvas[CANVAS_W * CANVAS_H];
    int    tool;       /* 0=pen 1=eraser 2=fill 3=line */
    uint32_t color;    /* aktif renk */
    int    brush_size;
    bool   drawing;
    int    last_x, last_y;
    int    line_x0, line_y0;
    Paint();
    void DrawContent() override;
    void OnClickContent(int mx,int my) override;
    bool HandleClick(int mx,int my,bool press) override;
private:
    void DrawToolbar();
    void DrawPalette();
    void DrawCanvas();
    int  CanvasX(int mx){ return mx-(x+2); }
    int  CanvasY(int my){ return my-(y+26+36); }
    void PaintPixel(int cx,int cy);
    void FloodFill(int cx,int cy,uint32_t old_col,uint32_t new_col);
};
#endif
