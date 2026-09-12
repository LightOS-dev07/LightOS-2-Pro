#ifndef PONG_H
#define PONG_H
#include "../../gui.h"

struct Pong : public Window {
    int bx,by,bdx,bdy;
    int p1y,p2y;
    int s1,s2;
    int pw,ph,bsz,gw,gh;
    bool started;

    Pong():Window("Pong",0,0,420,280){
        gw=w-24; gh=h-60; pw=10; ph=50; bsz=10;
        reset();
    }
    void reset(){
        bx=gw/2; by=gh/2;
        bdx=3; bdy=2;
        p1y=gh/2-ph/2; p2y=gh/2-ph/2;
        s1=0; s2=0; started=false;
    }
    void step(){
        if(!started) return;
        bx+=bdx; by+=bdy;
        if(by<=0){by=0;bdy=-bdy;}
        if(by>=gh-bsz){by=gh-bsz;bdy=-bdy;}
        // P1 paddle
        if(bx<=pw+10&&by+bsz>=p1y&&by<=p1y+ph){bx=pw+10;bdx=-bdx;bdy+=(by+bsz/2-(p1y+ph/2))/8;}
        // P2 paddle
        if(bx>=gw-pw-bsz-10&&by+bsz>=p2y&&by<=p2y+ph){bx=gw-pw-bsz-10;bdx=-bdx;}
        // AI for p2
        if(by+bsz/2>p2y+ph/2+2) p2y+=3; else if(by+bsz/2<p2y+ph/2-2) p2y-=3;
        if(p2y<0)p2y=0; if(p2y>gh-ph)p2y=gh-ph;
        // Score
        if(bx<0){s2++;reset();} if(bx>gw){s1++;reset();}
    }
    void DrawContent() override {
        int ox=x+2,oy=y+26;
        DrawRect(ox,oy,w-4,h-28,0x080818);
        int gx=ox+10,gy=oy+10;
        DrawRect(gx,gy,gw,gh,0x060614);
        DrawRectBorder(gx,gy,gw,gh,0x2233AA);
        // Center line
        for(int i=0;i<gh;i+=16) DrawRect(gx+gw/2-1,gy+i,2,8,0x223344);
        // Paddles
        DrawRect(gx+6,gy+p1y,pw,ph,0x44AAFF);
        DrawRect(gx+gw-pw-6,gy+p2y,pw,ph,0xFF6644);
        // Ball
        DrawRect(gx+bx,gy+by,bsz,bsz,0xFFFFFF);
        DrawRect(gx+bx+2,gy+by+2,bsz-4,bsz-4,0xCCCCFF);
        // Score
        char sc[8]; auto pn=[&](int v)->const char*{
            static char b[8];int i=0;if(!v){b[0]='0';b[1]=0;return b;}
            char t[8];int j=0;while(v>0){t[j++]='0'+v%10;v/=10;}
            for(int k=0;k<j;k++)b[k]=t[j-1-k];b[j]=0;return b;
        };
        DrawString(gx+gw/2-40,gy+8,pn(s1),0x44AAFF);
        DrawString(gx+gw/2+24,gy+8,pn(s2),0xFF6644);
        if(!started) DrawString(gx+gw/2-44,gy+gh/2-4,"SPACE to start",0xFFCC44);
        DrawString(ox+10,oy+gh+14,"W/S = move   AI = right",0x4466AA);
    }
    void KeyPress(unsigned int cp){
        if(cp==' '){started=true;}
        if(cp=='w'||cp==0xFF03) p1y-=18;
        if(cp=='s'||cp==0xFF04) p1y+=18;
        if(p1y<0)p1y=0; if(p1y>gh-ph)p1y=gh-ph;
    }
    void Tick(){if(started)step();}
};
#endif
