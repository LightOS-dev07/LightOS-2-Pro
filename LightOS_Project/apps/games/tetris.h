#ifndef TETRIS_H
#define TETRIS_H
#include "../../gui.h"

struct Tetris : public Window {
    static const int COLS=10,ROWS=20,CS=14;
    uint8_t board[ROWS][COLS];
    int px,py,ptype,prot;
    int score,lines,level,timer,speed;
    bool dead,started;

    static const int PIECES[7][4][4][2];

    Tetris():Window("Tetris",0,0,COLS*CS+120,ROWS*CS+50){reset();}

    void reset(){
        for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++) board[r][c]=0;
        score=0;lines=0;level=1;timer=0;speed=30;dead=false;started=false;
        spawn();
    }
    int rnd(){static unsigned s=99991;s=s*22695477+1;return(int)(s>>16)&0x7FFF;}
    void spawn(){ptype=rnd()%7;prot=0;px=COLS/2-1;py=0;}
    bool fits(int x,int y,int t,int r){
        for(int i=0;i<4;i++){int cx=x+PIECES[t][r][i][0],cy=y+PIECES[t][r][i][1];if(cx<0||cx>=COLS||cy<0||cy>=ROWS||board[cy][cx])return false;}return true;
    }
    void lock(){
        for(int i=0;i<4;i++){int cx=px+PIECES[ptype][prot][i][0],cy=py+PIECES[ptype][prot][i][1];if(cy>=0&&cy<ROWS)board[cy][cx]=(uint8_t)(ptype+1);}
        clear_lines(); spawn(); if(!fits(px,py,ptype,prot)) dead=true;
    }
    void clear_lines(){
        int cleared=0;
        for(int r=ROWS-1;r>=0;r--){
            bool full=true;for(int c=0;c<COLS;c++) if(!board[r][c]){full=false;break;}
            if(full){for(int rr=r;rr>0;rr--) for(int c=0;c<COLS;c++) board[rr][c]=board[rr-1][c];for(int c=0;c<COLS;c++) board[0][c]=0;cleared++;r++;}
        }
        if(cleared){lines+=cleared;score+=cleared*100*level;level=1+lines/10;speed=30-level*2;if(speed<4)speed=4;}
    }
    void step(){
        if(!fits(px,py+1,ptype,prot)) lock(); else py++;
    }
    static const uint32_t COLORS[8];
    void DrawContent() override {
        int ox=x+2,oy=y+26;
        DrawRect(ox,oy,w-4,h-28,0x080810);
        int gx=ox+10,gy=oy+10;
        DrawRect(gx,gy,COLS*CS,ROWS*CS,0x060610);
        DrawRectBorder(gx,gy,COLS*CS,ROWS*CS,0x223366);
        // Board
        for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++) if(board[r][c]){
            DrawRect(gx+c*CS+1,gy+r*CS+1,CS-2,CS-2,COLORS[board[r][c]]);
            DrawBevel(gx+c*CS+1,gy+r*CS+1,CS-2,CS-2,true);
        }
        // Current piece
        if(!dead&&started) for(int i=0;i<4;i++){
            int cx=px+PIECES[ptype][prot][i][0],cy=py+PIECES[ptype][prot][i][1];
            if(cy>=0){DrawRect(gx+cx*CS+1,gy+cy*CS+1,CS-2,CS-2,COLORS[ptype+1]);DrawBevel(gx+cx*CS+1,gy+cy*CS+1,CS-2,CS-2,true);}
        }
        // Side panel
        int sx=gx+COLS*CS+8;
        DrawString(sx,gy+4,"Score",0x8899CC);
        char buf[16]; auto pn=[&](int v){static char b[12];int i=0;if(!v){b[0]='0';b[1]=0;return(const char*)b;}char t[12];int j=0;while(v>0){t[j++]='0'+v%10;v/=10;}for(int k=0;k<j;k++)b[k]=t[j-1-k];b[j]=0;return(const char*)b;};
        DrawString(sx,gy+16,pn(score),0xFFFF44);
        DrawString(sx,gy+36,"Lines",0x8899CC);DrawString(sx,gy+48,pn(lines),0x44FFFF);
        DrawString(sx,gy+68,"Level",0x8899CC);DrawString(sx,gy+80,pn(level),0xFF8844);
        DrawString(sx,gy+100,"W=rot",0x6677AA);DrawString(sx,gy+112,"A/D mv",0x6677AA);DrawString(sx,gy+124,"S=drop",0x6677AA);
        if(dead){DrawRect(gx+2,gy+ROWS*CS/2-12,COLS*CS-4,24,0xAA0000);DrawString(gx+8,gy+ROWS*CS/2-6,"GAME OVER",0xFFFFFF);DrawString(gx+8,gy+ROWS*CS/2+6,"R=restart",0xFFCC44);}
        if(!started)DrawString(gx+4,gy+ROWS*CS/2-4,"SPACE=start",0xFFCC44);
    }
    void KeyPress(unsigned int cp){
        if(dead&&cp=='r'){reset();return;}
        if(cp==' '){started=true;return;}
        if(!started)return;
        if((cp=='a'||cp==0xFF01)&&fits(px-1,py,ptype,prot))px--;
        if((cp=='d'||cp==0xFF02)&&fits(px+1,py,ptype,prot))px++;
        if((cp=='s'||cp==0xFF04)&&fits(px,py+1,ptype,prot))py++;
        if((cp=='w'||cp==0xFF03)){int nr=(prot+1)%4;if(fits(px,py,ptype,nr))prot=nr;}
    }
    void Tick(){if(started&&!dead&&++timer>=speed){timer=0;step();}}
};

const uint32_t Tetris::COLORS[8]={0,0xFF4444,0xFF8800,0xFFFF00,0x44FF44,0x44AAFF,0x0000FF,0xAA00FF};
const int Tetris::PIECES[7][4][4][2]={
    {{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}}}, // O
    {{{0,0},{1,0},{2,0},{3,0}},{{1,0},{1,1},{1,2},{1,3}},{{0,1},{1,1},{2,1},{3,1}},{{0,0},{0,1},{0,2},{0,3}}}, // I
    {{{0,0},{1,0},{1,1},{2,1}},{{1,0},{0,1},{1,1},{0,2}},{{0,0},{1,0},{1,1},{2,1}},{{1,0},{0,1},{1,1},{0,2}}}, // S
    {{{1,0},{2,0},{0,1},{1,1}},{{0,0},{0,1},{1,1},{1,2}},{{1,0},{2,0},{0,1},{1,1}},{{0,0},{0,1},{1,1},{1,2}}}, // Z
    {{{0,0},{0,1},{1,1},{2,1}},{{1,0},{2,0},{1,1},{1,2}},{{0,0},{1,0},{2,0},{2,1}},{{1,0},{1,1},{0,2},{1,2}}}, // L
    {{{2,0},{0,1},{1,1},{2,1}},{{1,0},{1,1},{1,2},{2,2}},{{0,0},{1,0},{2,0},{0,1}},{{0,0},{1,0},{1,1},{1,2}}}, // J
    {{{1,0},{0,1},{1,1},{2,1}},{{1,0},{1,1},{2,1},{1,2}},{{0,1},{1,1},{2,1},{1,2}},{{1,0},{0,1},{1,1},{1,2}}}, // T
};
#endif
