#include "paint.h"

static const uint32_t PALETTE[16]={
    0x000000,0xFFFFFF,0xFF0000,0x00FF00,
    0x0000FF,0xFFFF00,0xFF00FF,0x00FFFF,
    0x888888,0xCCCCCC,0xAA4400,0x44AA00,
    0x0044AA,0xAA8800,0x880088,0x008888,
};

Paint::Paint():Window("Paint",50,30,330,282){
    for(int i=0;i<CANVAS_W*CANVAS_H;i++) canvas[i]=0xFFFFFF;
    tool=0; color=0x000000; brush_size=2;
    drawing=false; last_x=-1; last_y=-1; line_x0=0; line_y0=0;
}

void Paint::PaintPixel(int cx,int cy){
    if(tool==2){ /* fill */
        if(cx<0||cy<0||cx>=CANVAS_W||cy>=CANVAS_H) return;
        uint32_t old=canvas[cy*CANVAS_W+cx];
        if(old!=color) FloodFill(cx,cy,old,color);
        return;
    }
    uint32_t c=(tool==1)?0xFFFFFF:color;
    int r=brush_size;
    for(int dy=-r;dy<=r;dy++) for(int dx=-r;dx<=r;dx++){
        int px=cx+dx,py=cy+dy;
        if(px>=0&&py>=0&&px<CANVAS_W&&py<CANVAS_H)
            canvas[py*CANVAS_W+px]=c;
    }
}

void Paint::FloodFill(int cx,int cy,uint32_t old_col,uint32_t new_col){
    if(cx<0||cy<0||cx>=CANVAS_W||cy>=CANVAS_H) return;
    if(canvas[cy*CANVAS_W+cx]!=old_col) return;
    if(old_col==new_col) return;
    /* Yığın tabanlı fill */
    static int stack_x[8192],stack_y[8192]; int sp=0;
    stack_x[sp]=cx; stack_y[sp]=cy; sp++;
    while(sp>0){
        sp--;
        int sx=stack_x[sp],sy=stack_y[sp];
        if(sx<0||sy<0||sx>=CANVAS_W||sy>=CANVAS_H) continue;
        if(canvas[sy*CANVAS_W+sx]!=old_col) continue;
        canvas[sy*CANVAS_W+sx]=new_col;
        if(sp<8188){
            stack_x[sp]=sx+1;stack_y[sp]=sy;sp++;
            stack_x[sp]=sx-1;stack_y[sp]=sy;sp++;
            stack_x[sp]=sx;stack_y[sp]=sy+1;sp++;
            stack_x[sp]=sx;stack_y[sp]=sy-1;sp++;
        }
    }
}

void Paint::DrawToolbar(){
    DrawGradientV(x+2,y+26,w-4,20,0xECE9D8,0xD4D0C8);
    DrawRect(x+2,y+45,w-4,1,0xA09888);
    const char* tools[]={"Pen","Erase","Fill","Line"};
    for(int i=0;i<4;i++){
        bool sel=(tool==i);
        DrawGradientV(x+5+i*54,y+28,50,16,
            sel?TP().title_foc_top:0xECE9D8,
            sel?TP().title_foc_bot:0xD4D0C8);
        DrawBevel(x+5+i*54,y+28,50,16,sel);
        DrawString(x+12+i*54,y+32,tools[i],sel?TP().title_foc_text:0x000000);
    }
    /* Brush size */
    DrawString(x+222,y+32,"Sz:",0x505050);
    DrawGradientV(x+244,y+28,14,16,0xECE9D8,0xD4D0C8);
    DrawBevel(x+244,y+28,14,16,true);
    DrawString(x+247,y+32,"-",0x000000);
    char bsz[3]; bsz[0]='0'+brush_size; bsz[1]=0;
    DrawString(x+262,y+32,bsz,0x000080);
    DrawGradientV(x+274,y+28,14,16,0xECE9D8,0xD4D0C8);
    DrawBevel(x+274,y+28,14,16,true);
    DrawString(x+277,y+32,"+",0x000000);
}

void Paint::DrawPalette(){
    int py=y+26+20+4;
    DrawGradientV(x+2,py,w-4,16,0xD4D0C8,0xC4C0B8);
    DrawRect(x+2,py+15,w-4,1,0xA09888);
    /* Aktif renk kutusu */
    DrawRect(x+5,py+2,14,12,color);
    DrawRectBorder(x+5,py+2,14,12,0x000000);
    DrawString(x+22,py+4,"Color:",0x505050);
    /* Palet */
    for(int i=0;i<16;i++){
        int px2=x+70+i*16;
        DrawRect(px2,py+2,14,12,PALETTE[i]);
        DrawRectBorder(px2,py+2,14,12,0x888880);
        if(PALETTE[i]==color) DrawRectBorder(px2-1,py+1,16,14,0xFF0000);
    }
}

void Paint::DrawCanvas(){
    int ox=x+2, oy=y+26+36;
    /* Canvas piksellerini backbuffer'a kopyala */
    for(int cy=0;cy<CANVAS_H;cy++){
        for(int cx2=0;cx2<CANVAS_W;cx2++){
            int bx=ox+cx2, by=oy+cy;
            if(bx>=(int)SW||by>=(int)SH) continue;
            backbuffer[by*SW+bx]=canvas[cy*CANVAS_W+cx2];
        }
    }
    DrawRectBorder(ox-1,oy-1,CANVAS_W+2,CANVAS_H+2,0x888888);
}

void Paint::DrawContent(){
    DrawToolbar();
    DrawPalette();
    DrawCanvas();
}

bool Paint::HandleClick(int mx,int my,bool press){
    if(!visible||minimized) return false;
    /* Başlık */
    if(my>=y&&my<=y+25) return Window::HandleClick(mx,my,press);

    /* Toolbar */
    int ty2=y+26+2;
    if(my>=ty2&&my<=ty2+18&&press){
        for(int i=0;i<4;i++) if(mx>=x+5+i*54&&mx<=x+54+i*54){tool=i;return true;}
        if(mx>=x+244&&mx<=x+257){if(brush_size>1)brush_size--;return true;}
        if(mx>=x+274&&mx<=x+287){if(brush_size<8)brush_size++;return true;}
        return true;
    }

    /* Palet */
    int py=y+26+20+4;
    if(my>=py&&my<=py+16&&press){
        for(int i=0;i<16;i++){
            int px2=x+70+i*16;
            if(mx>=px2&&mx<=px2+14){color=PALETTE[i];return true;}
        }
        return true;
    }

    /* Canvas */
    int oy=y+26+36;
    int cx2=mx-(x+2), cy2=my-oy;
    if(cx2>=0&&cy2>=0&&cx2<CANVAS_W&&cy2<CANVAS_H){
        if(press){
            if(tool==3){/* line başlangıç */ line_x0=cx2;line_y0=cy2;}
            else PaintPixel(cx2,cy2);
            drawing=true; last_x=cx2; last_y=cy2;
        }
        if(drawing&&!press&&tool==3){
            /* Line: baştan sona */
            int dx=cx2-line_x0,dy=cy2-line_y0;
            int steps=dx<0?-dx:dx; if((dy<0?-dy:dy)>steps)steps=(dy<0?-dy:dy);
            if(steps>0) for(int s=0;s<=steps;s++)
                PaintPixel(line_x0+dx*s/steps,line_y0+dy*s/steps);
            drawing=false;
        } else if(drawing&&press&&tool!=3&&tool!=2&&last_x>=0){
            /* Sürekli çizim */
            int dx=cx2-last_x,dy=cy2-last_y;
            int steps=dx<0?-dx:dx; if((dy<0?-dy:dy)>steps)steps=(dy<0?-dy:dy);
            if(steps>0) for(int s=1;s<=steps;s++)
                PaintPixel(last_x+dx*s/steps,last_y+dy*s/steps);
            last_x=cx2; last_y=cy2;
        }
        return true;
    }
    if(!press) drawing=false;
    return false;
}

void Paint::OnClickContent(int mx,int my){ (void)mx;(void)my; }
