#ifndef TASKMAN_H
#define TASKMAN_H
/*
 * apps/taskman.h — Task Manager
 * ================================
 * Açık pencereler, bellek tahmini, CPU durumu.
 */
#include "../gui.h"
#include "../compositor.h"
#include "../kernel/clock.h"

extern uint32_t mem_upper_kb;
extern Compositor* g_comp_ptr; /* shell.cpp'de tanımlanacak */

struct TaskManager : public Window {
    int  sel;
    int  tick;
    bool kill_confirm;

    TaskManager():Window("Task Manager",80,60,360,280){
        sel=0; tick=0; kill_confirm=false;
    }

    void DrawContent() override {
        DrawRect(x+2,y+26,w-4,h-28,TP().win_content);

        /* Başlık şeridi */
        DrawGradientV(x+2,y+26,w-4,16,
            LerpColor(TP().win_content,WP_FrameFoc(),15,100),TP().win_content);
        DrawString(x+8, y+29,"Name",WP_TitleTop(true));
        DrawString(x+180,y+29,"Status",WP_TitleTop(true));
        DrawString(x+260,y+29,"Mem",WP_TitleTop(true));
        DrawRect(x+2,y+42,w-4,1,0xC0BCB4);

        /* Pencere listesi — SADECE gerçekten açık (visible) pencereler.
         * Önceden comp.count'taki TÜM pencereler (program başında bir
         * kez eklenen ~30 uygulama/oyun/dev-tool, hiç kapanmıyor) burada
         * "Running" olarak listeleniyordu — kullanıcı hiçbir şey açmasa
         * bile ekranda onlarca "Running" satırı görüyordu. Bu, RAM
         * göstergesiyle birleşince "sistem sürekli her şeyi çalıştırıyor
         * ve kaynak yiyor" izlenimini güçlendiriyordu. */
        if(!g_comp_ptr){ DrawString(x+8,y+50,"No compositor",0x888888); return; }
        Compositor& comp=*g_comp_ptr;
        int ly2=y+44;
        for(int i=comp.count-1;i>=0;i--){
            Window* ww=comp.Get(i);
            if(!ww->visible) continue;
            bool focused=(i==comp.count-1);
            bool issel=(sel==(comp.count-1-i));
            if(issel) DrawRect(x+2,ly2,w-4,18,LerpColor(TP().win_content,WP_FrameFoc(),20,100));
            /* İkon renk */
            uint32_t dot=focused?WP_FrameFoc():(ww->crashed?0xFF4444:0x8090A8);
            DrawRect(x+8,ly2+6,8,8,dot);
            DrawBevel(x+8,ly2+6,8,8,true);
            /* İsim (max 18 char) */
            char nm[20]; int ni=0;
            while(ww->title[ni]&&ni<18){nm[ni]=ww->title[ni];ni++;} nm[ni]=0;
            uint32_t tc=ww->crashed?0xFF4444:(focused?WP_TitleTop(true):0x404040);
            DrawString(x+20,ly2+5,nm,tc);
            /* Durum */
            const char* st=ww->crashed?"CRASHED":(ww->minimized?"Minimized":(focused?"Active":"Running"));
            uint32_t sc=ww->crashed?0xFF4444:(focused?0x008800:0x606060);
            DrawString(x+180,ly2+5,st,sc);
            /* Yaklaşık bellek (pencere boyutuna göre) */
            char mem[12]; int mi2=0;
            int kb=(ww->w*ww->h*4)/1024+4; /* backbuffer tahmin */
            int miv=kb;
            char tmp[8];int ti=0;
            while(miv>0){tmp[ti++]='0'+miv%10;miv/=10;} if(!ti)tmp[ti++]='0';
            for(int k=ti-1;k>=0;k--) mem[mi2++]=tmp[k];
            mem[mi2++]='K'; mem[mi2]=0;
            DrawString(x+260,ly2+5,mem,0x606060);
            ly2+=18;
            if(ly2>y+h-50) break;
        }

        /* Bellek çubuğu */
        int by=y+h-44;
        DrawRect(x+2,by,w-4,1,0xC0BCB4);
        DrawString(x+8,by+4,"RAM:",WP_TitleTop(true));
        int bar_w=w-80;
        uint32_t total_kb=mem_upper_kb?mem_upper_kb:131072; /* 128MB varsayılan */
        /* Kullanılan bellek tahmini: GERÇEKTEN AÇIK (visible) pencere
         * sayısına göre, her biri ~2MB varsayımıyla + sabit çekirdek payı.
         *
         * Önceki sürüm g_comp_ptr->count kullanıyordu — bu, compositor'a
         * program başında bir kez eklenen TOPLAM pencere sayısıydı (tüm
         * uygulamalar + oyunlar + dev tools, ~30 adet) ve pencere açılıp
         * kapandıkça HİÇ değişmiyordu (Compositor'da bir Remove() yok,
         * zaten olması da gerekmiyor — sadece count yanlış şeyi
         * ölçüyordu). Sonuç: RAM göstergesi baştan sabit yüksek bir
         * sayıda donuyor, kullanıcı ne yaparsa yapsın hiç değişmiyordu —
         * "sistem sürekli RAM yiyor ve hiç düşmüyor" izlenimi tam olarak
         * buradan geliyordu. Gerçek bir heap/allocator olmadığı için
         * (bkz. proje genelinde malloc/new hiç kullanılmıyor) bu hâlâ
         * gerçek bir bellek ölçümü değil, ama en azından kullanıcının
         * yaptığı işle (pencere açma/kapama) tutarlı şekilde değişiyor. */
        int open_windows=0;
        if(g_comp_ptr) for(int wi=0; wi<g_comp_ptr->count; wi++)
            if(g_comp_ptr->Get(wi)->visible) open_windows++;
        uint32_t used_kb=(uint32_t)open_windows*2048 + 12288;
        if(used_kb>total_kb) used_kb=total_kb;
        int fill=(int)((uint64_t)bar_w*used_kb/total_kb);
        DrawRect(x+48,by+3,bar_w,12,0xD4D0C8);
        DrawRect(x+48,by+3,fill,12,fill>bar_w*3/4?0xFF4444:(fill>bar_w/2?0xFF8800:0x00AA44));
        DrawRectBorder(x+48,by+3,bar_w,12,0xA09888);
        char meminfo[32]; int mii=0;
        int umb=(int)(used_kb/1024),tmb=(int)(total_kb/1024);
        auto pn=[&](int v){char t[8];int i=0;while(v>0){t[i++]='0'+v%10;v/=10;}if(!i)t[i++]='0';for(int k=i-1;k>=0;k--)meminfo[mii++]=t[k];};
        pn(umb);meminfo[mii++]='/';pn(tmb);
        const char* s2="MB";while(*s2)meminfo[mii++]=*s2++;meminfo[mii]=0;
        DrawString(x+48+bar_w+4,by+4,meminfo,0x404040);

        /* End Task butonu */
        DrawGradientV(x+8,by+18,80,16,0xCC2200,0x881100);
        DrawBevel(x+8,by+18,80,16,true);
        DrawString(x+12,by+22,"End Task",0xFFFFFF);
        DrawGradientV(x+96,by+18,80,16,TP().btn_min_top,TP().btn_min_bot);
        DrawBevel(x+96,by+18,80,16,true);
        DrawString(x+100,by+22,"Refresh",0x000000);
    }

    void OnClickContent(int mx,int my) override {
        if(!g_comp_ptr) return;
        Compositor& comp=*g_comp_ptr;
        /* Pencere seç */
        int ly2=y+44;
        for(int i=comp.count-1;i>=0;i--){
            if(mx>=x+2&&mx<=x+w-2&&my>=ly2&&my<=ly2+18){
                sel=(comp.count-1-i); return;
            }
            ly2+=18; if(ly2>y+h-50) break;
        }
        /* End Task */
        int by=y+h-44;
        if(my>=by+18&&my<=by+34){
            if(mx>=x+8&&mx<=x+88){
                /* Seçili pencereyi kapat */
                int idx=comp.count-1-sel;
                if(idx>=0&&idx<comp.count){
                    comp.Get(idx)->Close();
                }
            }
            if(mx>=x+96&&mx<=x+176){
                /* Refresh: sadece redraw */
            }
        }
    }
};
#endif
