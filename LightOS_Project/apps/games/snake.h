#ifndef SNAKE_H
#define SNAKE_H
#include "../../gui.h"

struct Snake : public Window {
    static const int COLS=20, ROWS=15, CS=14;
    struct P{int x,y;};
    P   body[300]; int len;
    P   food;
    int dx,dy;
    bool dead, started;
    int  score, timer, speed;

    Snake():Window("Snake",0,0,COLS*CS+24,ROWS*CS+68){
        reset();
    }
    void reset(){
        len=3; dx=1; dy=0; score=0; dead=false; started=false; timer=0; speed=8;
        body[0]={10,7}; body[1]={9,7}; body[2]={8,7};
        spawn_food();
    }
    void spawn_food(){
        bool ok=false;
        while(!ok){
            food.x=1+rand()%(COLS-2); food.y=1+rand()%(ROWS-2);
            ok=true;
            for(int i=0;i<len;i++) if(body[i].x==food.x&&body[i].y==food.y){ok=false;break;}
        }
    }
    int rand(){static unsigned s=12345;s=s*1664525+1013904223;return(int)(s>>16)&0x7FFF;}
    void step(){
        if(dead||!started) return;
        P nx={body[0].x+dx,body[0].y+dy};
        if(nx.x<=0||nx.x>=COLS-1||nx.y<=0||nx.y>=ROWS-1){dead=true;return;}
        for(int i=0;i<len-1;i++) if(body[i].x==nx.x&&body[i].y==nx.y){dead=true;return;}
        bool ate=(nx.x==food.x&&nx.y==food.y);
        if(!ate) for(int i=len-1;i>0;i--) body[i]=body[i-1];
        else { if(len<299) len++; for(int i=len-1;i>0;i--) body[i]=body[i-1]; score+=10; speed=8-score/50; if(speed<2)speed=2; spawn_food(); }
        body[0]=nx;
    }
    void DrawContent() override {
        int ox=x+2, oy=y+26;
        DrawRect(ox,oy,w-4,h-28,0x0A1A0A);
        // Grid area
        int gx=ox+10, gy=oy+10;
        DrawRect(gx,gy,COLS*CS,ROWS*CS,0x081208);
        DrawRectBorder(gx,gy,COLS*CS,ROWS*CS,0x1A4A1A);
        // Food
        DrawRect(gx+food.x*CS+1,gy+food.y*CS+1,CS-2,CS-2,0xFF4422);
        DrawRect(gx+food.x*CS+3,gy+food.y*CS+3,CS-6,CS-6,0xFF8866);
        // Snake
        for(int i=0;i<len;i++){
            uint32_t col=(i==0)?0x44FF44:(i%2?0x22BB22:0x1A9A1A);
            DrawRect(gx+body[i].x*CS+1,gy+body[i].y*CS+1,CS-2,CS-2,col);
        }
        // Score
        DrawString(ox+10,oy+ROWS*CS+14,"Score:",0x44FF44);
        char sc[8]; int si=0,sv=score;
        if(!sv){sc[si++]='0';}while(sv>0){sc[si++]='0'+sv%10;sv/=10;}
        char sc2[8]; for(int i=0;i<si;i++) sc2[i]=sc[si-1-i]; sc2[si]=0;
        DrawString(ox+54,oy+ROWS*CS+14,sc2,0xFFFF44);
        if(dead){
            DrawRect(gx+COLS*CS/2-50,gy+ROWS*CS/2-10,100,20,0xAA0000);
            DrawString(gx+COLS*CS/2-38,gy+ROWS*CS/2-4,"GAME OVER",0xFFFFFF);
            DrawString(gx+COLS*CS/2-44,gy+ROWS*CS/2+12,"R=Restart",0xFFCC44);
        } else if(!started){
            DrawString(gx+COLS*CS/2-44,gy+ROWS*CS/2-4,"Press SPACE",0xFFCC44);
        }
    }
    void KeyPress(unsigned int cp){
        if(dead&&cp=='r'){reset();return;}
        if(cp==' '){started=true;return;}
        if(!started) return;
        if((cp==0xFF03||cp=='w')&&dy==0){dx=0;dy=-1;}
        if((cp==0xFF04||cp=='s')&&dy==0){dx=0;dy=1;}
        if((cp==0xFF01||cp=='a')&&dx==0){dx=-1;dy=0;}
        if((cp==0xFF02||cp=='d')&&dx==0){dx=1;dy=0;}
    }
    void Tick(){ if(++timer>=speed){timer=0;step();} }
};
#endif
