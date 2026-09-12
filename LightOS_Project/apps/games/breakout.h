#ifndef BREAKOUT_H
#define BREAKOUT_H
#include "../../gui.h"

struct Breakout : public Window {
    static const int GW=320, GH=220, BRICK_COLS=10, BRICK_ROWS=5;
    static const int BW=28, BH=10, BG=2; /* brick width/height/gap */

    float px,py;   /* top pozisyonu */
    float pdx,pdy; /* top hızı */
    int   padx;    /* paddle x (merkez) */
    int   padw=52, padh=10;
    bool  bricks[BRICK_ROWS][BRICK_COLS];
    int   score,lives;
    bool  started,dead,won;
    int   alive_count;

    static const uint32_t BRICK_COLORS[BRICK_ROWS];

    Breakout():Window("Breakout",0,0,GW+24,GH+80){ reset(); }

    void reset(){
        px=GW/2.0f; py=GH-40.0f;
        pdx=2.5f; pdy=-2.5f;
        padx=GW/2;
        score=0; lives=3; started=false; dead=false; won=false;
        alive_count=BRICK_COLS*BRICK_ROWS;
        for(int r=0;r<BRICK_ROWS;r++)
            for(int c=0;c<BRICK_COLS;c++) bricks[r][c]=true;
    }

    void step(){
        if(!started||dead||won) return;
        px+=pdx; py+=pdy;

        /* Duvar çarpması */
        if(px<=4)      { px=4;      pdx=-pdx; }
        if(px>=GW-4)   { px=GW-4;  pdx=-pdx; }
        if(py<=4)      { py=4;      pdy=-pdy; }

        /* Top düştü */
        if(py>=GH){ lives--; if(lives<=0){dead=true;return;} reset_ball(); return; }

        /* Paddle çarpması */
        int px_i=(int)px, py_i=(int)py;
        if(py_i>=GH-28&&py_i<=GH-18&&
           px_i>=padx-padw/2&&px_i<=padx+padw/2){
            pdy=-pdy;
            pdx+=((px-padx)/(padw/2.0f))*0.8f; /* açı değişimi */
            if(pdx>4.5f)pdx=4.5f; if(pdx<-4.5f)pdx=-4.5f;
        }

        /* Tuğla çarpması */
        int bstart_x=12, bstart_y=24;
        for(int r=0;r<BRICK_ROWS;r++){
            for(int c=0;c<BRICK_COLS;c++){
                if(!bricks[r][c]) continue;
                int bx=bstart_x+c*(BW+BG);
                int by=bstart_y+r*(BH+BG);
                if(px_i>=bx&&px_i<=bx+BW&&py_i>=by&&py_i<=by+BH){
                    bricks[r][c]=false; alive_count--;
                    score+=10*(BRICK_ROWS-r);
                    pdy=-pdy;
                    if(alive_count<=0){won=true;return;}
                }
            }
        }
    }

    void reset_ball(){
        px=padx; py=GH-50.0f;
        pdx=2.5f; pdy=-2.5f;
        started=false;
    }

    void DrawContent() override {
        int ox=x+2,oy=y+26;
        DrawRect(ox,oy,w-4,h-28,0x0A080F);
        int gx=ox+2,gy=oy+2;
        DrawRect(gx,gy,GW,GH,0x08060D);
        DrawRectBorder(gx,gy,GW,GH,0x2A1A4A);

        /* Tuğlalar */
        int bstart_x=12, bstart_y=24;
        for(int r=0;r<BRICK_ROWS;r++)
            for(int c=0;c<BRICK_COLS;c++){
                if(!bricks[r][c]) continue;
                int bx=gx+bstart_x+c*(BW+BG);
                int by=gy+bstart_y+r*(BH+BG);
                DrawGradientV(bx,by,BW,BH,
                    BrightColor(BRICK_COLORS[r],120),
                    DimColor(BRICK_COLORS[r],80));
                DrawBevel(bx,by,BW,BH,true);
            }

        /* Paddle */
        DrawGradientV(gx+padx-padw/2,gy+GH-28,padw,padh,0x4488FF,0x2255CC);
        DrawBevel(gx+padx-padw/2,gy+GH-28,padw,padh,true);

        /* Top */
        DrawRect(gx+(int)px-4,gy+(int)py-4,8,8,0xFFEE44);
        DrawBevel(gx+(int)px-4,gy+(int)py-4,8,8,true);

        /* HUD */
        int hy=gy+GH+4;
        DrawRect(ox,hy,w-4,42,0x0A080F);
        DrawString(gx+4,hy+8,"Score:",0x8899CC);
        auto pn=[&](int v,int px2,int py2,uint32_t col){
            char buf[12];int i=0;if(!v){buf[i++]='0';}
            char t[12];int j=0;while(v>0){t[j++]='0'+v%10;v/=10;}
            for(int k=j-1;k>=0;k--)buf[i++]=t[k];buf[i]=0;
            DrawString(px2,py2,buf,col);
        };
        pn(score,gx+56,hy+8,0xFFFF44);
        DrawString(gx+120,hy+8,"Lives:",0x8899CC);
        for(int i=0;i<lives&&i<5;i++)
            DrawRect(gx+168+i*16,hy+10,10,10,0xFF4444);
        DrawString(gx+GW-80,hy+8,"A/D=move",0x445566);

        if(dead){
            DrawRect(gx+GW/2-55,gy+GH/2-14,110,28,0xAA0000);
            DrawString(gx+GW/2-44,gy+GH/2-8,"GAME OVER",0xFFFFFF);
            DrawString(gx+GW/2-36,gy+GH/2+8,"R=restart",0xFFCC44);
        } else if(won){
            DrawRect(gx+GW/2-48,gy+GH/2-14,96,28,0x00AA44);
            DrawString(gx+GW/2-36,gy+GH/2-8,"YOU WIN!",0xFFFFFF);
        } else if(!started){
            DrawString(gx+GW/2-52,gy+GH/2-4,"SPACE to launch",0xFFCC44);
        }
    }

    void KeyPress(unsigned int cp){
        if(dead&&cp=='r'){reset();return;}
        if(cp==' '){started=true;return;}
        if(cp=='a'||cp==0xFF01) padx-=20;
        if(cp=='d'||cp==0xFF02) padx+=20;
        if(padx<padw/2) padx=padw/2;
        if(padx>GW-padw/2) padx=GW-padw/2;
        if(!started){px=padx;}
    }
    void Tick(){ step(); }
};

const uint32_t Breakout::BRICK_COLORS[BRICK_ROWS]={
    0xFF4444,0xFF8800,0xFFFF00,0x44FF44,0x4488FF
};
#endif
