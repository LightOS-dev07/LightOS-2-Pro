#ifndef GAMES_FOLDER_H
#define GAMES_FOLDER_H
/*
 * XaefGAMES klasör penceresi
 * 4 oyun ikonunu gösterir, tıklayınca açar
 */
#include "../../gui.h"

struct GamesFolder : public Window {
    int hovered;
    GamesFolder():Window("XaefGAMES",100,80,320,220){
        hovered=-1; last_launch=-1;
        for(int i=0;i<4;i++) game_wins[i]=nullptr;
    }

    struct GameIcon {
        const char* name;
        uint32_t    col1,col2;
        const char* symbol;
    };
    static const GameIcon ICONS[4];

    void DrawContent() override {
        DrawRect(x+2,y+26,w-4,h-28,0x101018);
        DrawGradientV(x+2,y+26,w-4,16,
            LerpColor(0x101018,0x2244AA,20,100),0x101018);
        DrawString(x+8,y+29,"XaefGAMES",0x4488FF);
        DrawString(x+w-70,y+29,"by Xaef BTL",0x2244AA);

        int ix=x+14, iy=y+52;
        int iw=60,ih=68,gap=10;
        for(int i=0;i<4;i++){
            int cx=ix+(iw+gap)*i;
            bool hov=(i==hovered);
            if(hov) DrawRect(cx-4,iy-4,iw+8,ih+8,LerpColor(0x101018,0x2244AA,30,100));
            DrawGradientV(cx,iy,iw,iw,ICONS[i].col1,ICONS[i].col2);
            DrawRectBorder(cx,iy,iw,iw,hov?0x88AAFF:LerpColor(ICONS[i].col1,0xFFFFFF,30,100));
            /* Oyun sembolü */
            DrawStringCentered(cx+iw/2,iy+iw/2-6,ICONS[i].symbol,0xFFFFFF);
            /* İsim */
            DrawStringCentered(cx+iw/2,iy+iw+6,ICONS[i].name,hov?0x88AAFF:0xCCDDFF);
        }
        DrawString(x+8,y+h-20,"Click icon to launch",0x334466);
    }

    void OnClickContent(int mx,int my) override {
        int ix=x+14,iy=y+52,iw=60,gap=10;
        for(int i=0;i<4;i++){
            int cx=ix+(iw+gap)*i;
            if(mx>=cx&&mx<=cx+iw&&my>=iy&&my<=iy+iw){
                launch(i); return;
            }
        }
    }

    void UpdateHover(int mx,int my){
        hovered=-1;
        if(!visible||minimized) return;
        int ix=x+14,iy=y+52,iw=60,gap=10;
        for(int i=0;i<4;i++){
            int cx=ix+(iw+gap)*i;
            if(mx>=cx&&mx<=cx+iw&&my>=iy&&my<=iy+iw){hovered=i;return;}
        }
    }

    Window* game_wins[4];   /* shell tarafından doldurulur */
    int last_launch;
    void launch(int i){
        if(i>=0&&i<4&&game_wins[i]){
            Window* gw=game_wins[i];
            gw->x=(800-gw->w)/2;
            gw->y=(600-58-gw->h)/2;
            gw->Open();
            last_launch=i;
        }
    }
};

const GamesFolder::GameIcon GamesFolder::ICONS[4]={
    {"Snake",   0x0A3A0A,0x051805,"[o]"},
    {"Tetris",  0x1A0A3A,0x0A0520,"[T]"},
    {"Pong",    0x1A1A0A,0x0A0A05,"[|]"},
    {"Mines",   0x3A0A0A,0x200505,"[*]"},
};
#endif
