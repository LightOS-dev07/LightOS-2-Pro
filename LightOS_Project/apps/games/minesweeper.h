#ifndef MINESWEEPER_H
#define MINESWEEPER_H
#include "../../gui.h"

struct Minesweeper : public Window {
    static const int COLS=16,ROWS=12,MINES=30,CS=22;
    struct Cell{ bool mine,revealed,flagged; uint8_t adj; };
    Cell board[ROWS][COLS];
    int  flagged_count,revealed_count;
    bool dead,won,first_click;
    int  timer_sec;

    Minesweeper():Window("Minesweeper",0,0,COLS*CS+24,ROWS*CS+76){reset();}

    int rnd(){static unsigned s=54321;s=s*1664525+1013904223;return(int)(s>>16)&0x7FFF;}

    void reset(){
        for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++) board[r][c]={false,false,false,0};
        flagged_count=0;revealed_count=0;dead=false;won=false;first_click=true;timer_sec=0;
    }

    void place_mines(int sx,int sy){
        int placed=0;
        while(placed<MINES){
            int r=rnd()%ROWS,c=rnd()%COLS;
            if(!board[r][c].mine&&!(r==sy&&c==sx)){board[r][c].mine=true;placed++;}
        }
        for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++){
            if(board[r][c].mine) continue;
            int cnt=0;
            for(int dr=-1;dr<=1;dr++) for(int dc=-1;dc<=1;dc++){
                int nr=r+dr,nc=c+dc;if(nr>=0&&nr<ROWS&&nc>=0&&nc<COLS&&board[nr][nc].mine)cnt++;
            }
            board[r][c].adj=(uint8_t)cnt;
        }
    }

    void reveal(int r,int c){
        if(r<0||r>=ROWS||c<0||c>=COLS||board[r][c].revealed||board[r][c].flagged) return;
        board[r][c].revealed=true; revealed_count++;
        if(board[r][c].adj==0&&!board[r][c].mine)
            for(int dr=-1;dr<=1;dr++) for(int dc=-1;dc<=1;dc++) reveal(r+dr,c+dc);
    }

    static const uint32_t NUM_COLORS[9];
    void DrawContent() override {
        int ox=x+2,oy=y+26;
        DrawRect(ox,oy,w-4,h-28,0xC0C0C0);
        // Header
        DrawGradientV(ox,oy,w-4,28,0xE0E0E0,0xC0C0C0);
        DrawBevel(ox,oy,w-4,28,true);
        // Mine counter
        char mc[6]; int mi=MINES-flagged_count,mabs=mi<0?-mi:mi;
        mc[0]=mi<0?'-':' '; int mj=1; if(!mabs)mc[mj++]='0';
        char mt[4];int mk=0;while(mabs>0){mt[mk++]='0'+mabs%10;mabs/=10;}
        for(int k=mk-1;k>=0;k--)mc[mj++]=mt[k]; mc[mj]=0;
        DrawRect(ox+8,oy+4,40,20,0x000000);DrawString(ox+10,oy+8,mc,0xFF2222);
        // Timer
        DrawRect(ox+w-52,oy+4,40,20,0x000000);
        char tc[8]; int tv=timer_sec,tj=0;
        char tt[8];int tk=0;if(!tv)tt[tk++]='0';while(tv>0){tt[tk++]='0'+tv%10;tv/=10;}
        for(int k=tk-1;k>=0;k--)tc[tj++]=tt[k];tc[tj]=0;
        DrawString(ox+w-50,oy+8,tc,0xFF2222);
        // Reset button
        DrawGradientV(ox+w/2-14,oy+4,28,20,0xE0E0E0,0xC8C8C8);
        DrawBevel(ox+w/2-14,oy+4,28,20,true);
        DrawString(ox+w/2-6,oy+8,dead?"X_X":(won?":D":":)"),dead?0xFF2222:(won?0xFFAA00:0x000000));
        // Grid
        int gx=ox+12,gy=oy+32;
        DrawRect(gx,gy,COLS*CS,ROWS*CS,0xC0C0C0);
        for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++){
            int cx=gx+c*CS,cy=gy+r*CS;
            Cell& cl=board[r][c];
            if(!cl.revealed){
                DrawGradientV(cx+1,cy+1,CS-2,CS-2,0xDDDDDD,0xBBBBBB);
                DrawBevel(cx+1,cy+1,CS-2,CS-2,true);
                if(cl.flagged) DrawString(cx+5,cy+6,"F",0xFF0000);
            } else {
                DrawRect(cx+1,cy+1,CS-2,CS-2,0xC8C8C8);
                if(cl.mine&&dead){DrawRect(cx+1,cy+1,CS-2,CS-2,0xFF4444);DrawString(cx+5,cy+5,"*",0x000000);}
                else if(cl.adj>0){char nc[2];nc[0]='0'+cl.adj;nc[1]=0;DrawString(cx+6,cy+5,nc,NUM_COLORS[cl.adj]);}
            }
        }
        if(!first_click&&!dead&&!won){
            int need=ROWS*COLS-MINES;
            if(revealed_count>=need){won=true;}
        }
        if(won){DrawRect(gx+COLS*CS/2-50,gy+ROWS*CS/2-10,100,20,0x00AA00);DrawString(gx+COLS*CS/2-30,gy+ROWS*CS/2-4,"YOU WIN!",0xFFFFFF);}
    }

    void OnClickContent(int mx,int my) override {
        // Reset button
        int ox=x+2,oy=y+26;
        if(mx>=ox+w/2-14&&mx<=ox+w/2+14&&my>=oy+4&&my<=oy+24){reset();return;}
        if(dead||won) return;
        int gx=ox+12,gy=oy+32;
        if(mx<gx||mx>=gx+COLS*CS||my<gy||my>=gy+ROWS*CS) return;
        int c=(mx-gx)/CS,r=(my-gy)/CS;
        if(c<0||c>=COLS||r<0||r>=ROWS) return;
        Cell& cl=board[r][c];
        if(cl.revealed) return;
        // Right-click sim: if ctrl held — flag (simplified: use 'f' key)
        if(cl.flagged) return;
        if(first_click){place_mines(c,r);first_click=false;}
        if(cl.mine){cl.revealed=true;dead=true;for(int rr=0;rr<ROWS;rr++)for(int cc=0;cc<COLS;cc++)if(board[rr][cc].mine)board[rr][cc].revealed=true;return;}
        reveal(r,c);
    }
    void KeyPress(unsigned int cp){
        // 'f' = flag last hovered — simplified: not implemented
        if(cp=='r') reset();
    }
};
const uint32_t Minesweeper::NUM_COLORS[9]={0,0x0000FF,0x007700,0xFF0000,0x000088,0x880000,0x008888,0x000000,0x888888};
#endif
