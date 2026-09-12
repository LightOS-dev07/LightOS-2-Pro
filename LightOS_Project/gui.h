#ifndef GUI_H
#define GUI_H

#include "graphics.h"
#include "lang.h"
#include "theme.h"
#include "wallpaper.h"

static int  GUI_strlen(const char* s){int n=0;while(s[n])n++;return n;}
static void GUI_strcpy(char* d,const char* s){int i=0;while(s[i]){d[i]=s[i];i++;}d[i]=0;}

static void DrawButton3D(int x,int y,int w,int h,
                         uint32_t topC,uint32_t botC,
                         const char* lbl,uint32_t tc,bool pressed=false){
    DrawGradientV(x,y,w,h,pressed?botC:topC,pressed?topC:botC);
    DrawBevel(x,y,w,h,!pressed);
    int tw=StringWidth(lbl);
    DrawString(x+(w-tw)/2+(pressed?1:0),y+(h-8)/2+(pressed?1:0),lbl,tc);
}

/* ════════════════════════════════════════════
   PENCERE
   ════════════════════════════════════════════ */
struct Window {
    int  x,y,w,h;
    bool visible,minimized,dragging;
    bool maximized;
    int  restore_x,restore_y,restore_w,restore_h;
    char title[40];
    int  drag_off_x,drag_off_y;

    enum ResizeEdge{NONE=0,LEFT,RIGHT,TOP,BOTTOM,TL,TR,BL,BR};
    ResizeEdge resize_edge;
    int  resize_start_mx,resize_start_my;
    int  resize_start_x,resize_start_y,resize_start_w,resize_start_h;

    int  anim_frame;
    static const int ANIM_MAX=4;
    static const int MIN_W=160;
    static const int MIN_H=120;
    static const int EDGE=6;

    /* ── App crash koruması ── */
    bool crashed;
    char crash_msg[48];

    Window(const char* t,int _x,int _y,int _w,int _h){
        GUI_strcpy(title,t);
        x=_x;y=_y;w=_w;h=_h;
        visible=false;minimized=false;dragging=false;maximized=false;
        crashed=false; crash_msg[0]=0;
        resize_edge=NONE;
        restore_x=_x;restore_y=_y;restore_w=_w;restore_h=_h;
        drag_off_x=0;drag_off_y=0;anim_frame=0;
        resize_start_mx=0;resize_start_my=0;
        resize_start_x=0;resize_start_y=0;resize_start_w=0;resize_start_h=0;
    }
    void SetTitle(const char* t){GUI_strcpy(title,t);}
    void Open(){visible=true;minimized=false;anim_frame=0;crashed=false;}

    /* Pencere kapatılınca ghost kalmaz — state tam sıfırla */
    void Close(){
        visible=false;minimized=false;dragging=false;
        resize_edge=NONE;crashed=false;
        anim_frame=0;
    }

    void DoMaximize(){
        if(maximized){x=restore_x;y=restore_y;w=restore_w;h=restore_h;maximized=false;}
        else{restore_x=x;restore_y=y;restore_w=w;restore_h=h;x=0;y=0;w=SW;h=SH-58;maximized=true;}
    }

    ResizeEdge HitEdge(int mx,int my){
        if(maximized) return NONE;
        bool L=mx>=x&&mx<=x+EDGE,R=mx>=x+w-EDGE&&mx<=x+w;
        bool T=my>=y&&my<=y+EDGE,B=my>=y+h-EDGE&&my<=y+h;
        if(T&&L)return TL;if(T&&R)return TR;
        if(B&&L)return BL;if(B&&R)return BR;
        if(L)return LEFT;if(R)return RIGHT;
        if(T)return TOP;if(B)return BOTTOM;
        return NONE;
    }

    virtual void Draw(bool focused=true){
        if(!visible||minimized) return;

        /* Crash ekranı */
        if(crashed){
            DrawRect(x,y,w,h,0x1A0808);
            DrawRectBorder(x,y,w,h,0xFF4444);
            DrawGradientV(x+1,y+1,w-2,24,0x881111,0x440808);
            DrawString(x+7,y+8,title,0xFF8888);
            DrawString(x+w-21,y+6,"X",0xFFFFFF);
            DrawRect(x+2,y+26,w-4,h-28,0x0A0404);
            DrawString(x+8,y+36,"App Crashed",0xFF4444);
            DrawString(x+8,y+52,crash_msg,0xFF8888);
            DrawString(x+8,y+68,"Close and reopen to restart.",0xAA6666);
            return;
        }

        if(anim_frame<ANIM_MAX){
            int a=anim_frame+1;
            int aw=w*a/ANIM_MAX,ah=h*a/ANIM_MAX;
            DrawRectBorder(x+(w-aw)/2,y+(h-ah)/2,aw,ah,focused?WP_FrameFoc():TP().win_frame_unf);
            anim_frame++; return;
        }

        DrawRect(x,y,w,h,focused?WP_FrameFoc():TP().win_frame_unf);
        DrawRect(x+1,y+1,w-2,h-2,TP().win_body);
        uint32_t ttop=WP_TitleTop(focused),tbot=WP_TitleBot(focused);
        DrawGradientV(x+1,y+1,w-2,24,ttop,tbot);
        if(TP().ice_glass&&focused){
            DrawRect(x+1,y+3,w-2,1,LerpColor(ttop,0xFFFFFF,40,100));
            DrawRect(x+1,y+23,w-2,1,LerpColor(tbot,0x88AACC,20,100));
        }
        if(focused) DrawRect(x+1,y+1,w-2,1,WP_TitleShine());
        uint32_t tc=WP_TitleText(focused);
        DrawString(x+8,y+9,title,tc);
        /* Butonlar */
        DrawGradientV(x+w-21,y+4,17,15,TP().btn_close_top,TP().btn_close_bot);
        DrawBevel(x+w-21,y+4,17,15,true);
        DrawString(x+w-18,y+6,"X",0xFFFFFF);
        uint32_t mxTop=maximized?DimColor(TP().btn_min_top,80):TP().btn_min_top;
        uint32_t mxBot=maximized?DimColor(TP().btn_min_bot,80):TP().btn_min_bot;
        DrawGradientV(x+w-40,y+4,17,15,mxTop,mxBot);
        DrawBevel(x+w-40,y+4,17,15,true);
        if(maximized){DrawRectBorder(x+w-37,y+7,9,7,0x000000);DrawRectBorder(x+w-36,y+8,9,7,0xFFFFFF);}
        else DrawRectBorder(x+w-37,y+7,11,9,tc==0xFFFFFF?0xFFFFFF:0x000000);
        DrawGradientV(x+w-59,y+4,17,15,TP().btn_min_top,TP().btn_min_bot);
        DrawBevel(x+w-59,y+4,17,15,true);
        DrawString(x+w-56,y+7,"-",TP().ice_glass?0x1A3A5C:0x000000);
        DrawRect(x+1,y+25,w-2,1,focused?WP_FrameFoc():TP().win_frame_unf);
        DrawRect(x+2,y+26,w-4,h-28,TP().win_content);
        if(!maximized){
            uint32_t hc=focused?LerpColor(WP_FrameFoc(),0xFFFFFF,30,100):0xA0A8B0;
            DrawRect(x+w-5,y+h-2,3,2,hc);DrawRect(x+w-2,y+h-5,2,3,hc);
            DrawRect(x+w-9,y+h-2,3,2,hc);DrawRect(x+w-2,y+h-9,2,3,hc);
        }
        DrawContent();
    }

    virtual void DrawContent(){}
    virtual void OnClickContent(int mx,int my){}

    virtual bool HandleClick(int mx,int my,bool press){
        if(!visible||minimized) return false;
        if(mx<x||mx>x+w||my<y||my>y+h) return false;
        if(my>=y&&my<=y+25){
            if(press){
                if(mx>=x+w-21&&mx<=x+w-4){Close();return true;}
                if(mx>=x+w-40&&mx<=x+w-23){DoMaximize();return true;}
                if(mx>=x+w-59&&mx<=x+w-42){minimized=true;return true;}
                if(!maximized){dragging=true;drag_off_x=mx-x;drag_off_y=my-y;}
            }
            return true;
        }
        if(press&&!maximized){
            ResizeEdge e=HitEdge(mx,my);
            if(e!=NONE){
                resize_edge=e;
                resize_start_mx=mx;resize_start_my=my;
                resize_start_x=x;resize_start_y=y;
                resize_start_w=w;resize_start_h=h;
                return true;
            }
        }
        if(mx>=x&&mx<=x+w&&my>y+25&&my<=y+h){
            if(press&&!crashed) OnClickContent(mx,my);
            return true;
        }
        return false;
    }

    void UpdateDrag(int mx,int my,bool hold){
        if(!hold){dragging=false;resize_edge=NONE;return;}
        if(dragging){
            x=mx-drag_off_x;y=my-drag_off_y;
            if(x<0)x=0;if(y<0)y=0;
            if(x+w>(int)SW)x=SW-w;
            if(y+h>(int)(SH-62))y=SH-62-h;
            return;
        }
        if(resize_edge==NONE) return;
        int ddx=mx-resize_start_mx,ddy=my-resize_start_my;
        int nx=resize_start_x,ny=resize_start_y,nw=resize_start_w,nh=resize_start_h;
        switch(resize_edge){
            case RIGHT: nw=resize_start_w+ddx; break;
            case LEFT:  nx=resize_start_x+ddx;nw=resize_start_w-ddx; break;
            case BOTTOM:nh=resize_start_h+ddy; break;
            case TOP:   ny=resize_start_y+ddy;nh=resize_start_h-ddy; break;
            case BR:    nw=resize_start_w+ddx;nh=resize_start_h+ddy; break;
            case BL:    nx=resize_start_x+ddx;nw=resize_start_w-ddx;nh=resize_start_h+ddy; break;
            case TR:    ny=resize_start_y+ddy;nw=resize_start_w+ddx;nh=resize_start_h-ddy; break;
            case TL:    nx=resize_start_x+ddx;ny=resize_start_y+ddy;nw=resize_start_w-ddx;nh=resize_start_h-ddy; break;
            default: break;
        }
        if(nw<MIN_W){if(resize_edge==LEFT||resize_edge==TL||resize_edge==BL)nx=resize_start_x+resize_start_w-MIN_W;nw=MIN_W;}
        if(nh<MIN_H){if(resize_edge==TOP||resize_edge==TL||resize_edge==TR)ny=resize_start_y+resize_start_h-MIN_H;nh=MIN_H;}
        if(nx<0)nx=0;if(ny<0)ny=0;
        if(nx+nw>(int)SW)nw=SW-nx;
        if(ny+nh>(int)(SH-62))nh=SH-62-ny;
        x=nx;y=ny;w=nw;h=nh;
    }
};

/* ── Basit MessageBox ── */
struct MsgBox {
    bool    visible;
    char    title[48];
    char    msg[128];
    int     x,y,w,h;

    MsgBox():visible(false){title[0]=0;msg[0]=0;
        w=320;h=120;x=(SW-w)/2;y=(SH-h)/2;
    }

    void Show(const char* t,const char* m){
        int i=0;while(t[i]&&i<47){title[i]=t[i];i++;}title[i]=0;
        i=0;while(m[i]&&i<127){msg[i]=m[i];i++;}msg[i]=0;
        visible=true;
        x=(SW-w)/2; y=(SH-h)/2;
    }

    void Draw(){
        if(!visible) return;
        /* Shadow */
        DrawRect(x+4,y+4,w,h,DimColor(0x000000,30));
        /* Body */
        DrawRect(x,y,w,h,0xF0EEE8);
        DrawRectBorder(x,y,w,h,0x808080);
        /* Title bar */
        DrawGradientV(x+1,y+1,w-2,22,0xCC3300,0x881100);
        DrawString(x+8,y+7,title,0xFFFFFF);
        /* Close button */
        DrawGradientV(x+w-20,y+3,16,16,0xFF5555,0xCC2222);
        DrawBevel(x+w-20,y+3,16,16,true);
        DrawString(x+w-17,y+5,"X",0xFFFFFF);
        /* Message */
        DrawRect(x+1,y+23,w-2,h-44,0xFFFFFF);
        DrawString(x+12,y+32,msg,0x202020);
        /* Warning icon */
        DrawString(x+12,y+48,"! The application crashed.",0xCC4400);
        /* OK button */
        DrawGradientV(x+w/2-30,y+h-26,60,20,0xE0DDD0,0xC8C4B8);
        DrawBevel(x+w/2-30,y+h-26,60,20,true);
        DrawString(x+w/2-10,y+h-20,"OK",0x000000);
    }

    /* true döndürürse kapat */
    bool Click(int mx,int my){
        if(!visible) return false;
        /* OK butonu */
        if(mx>=x+w/2-30&&mx<=x+w/2+30&&my>=y+h-26&&my<=y+h-6){
            visible=false; return true;
        }
        /* X butonu */
        if(mx>=x+w-20&&mx<=x+w-4&&my>=y+3&&my<=y+19){
            visible=false; return true;
        }
        /* Tıklamayı yut (modal) */
        return mx>=x&&mx<=x+w&&my>=y&&my<=y+h;
    }
};

extern MsgBox g_msgbox;
#endif
