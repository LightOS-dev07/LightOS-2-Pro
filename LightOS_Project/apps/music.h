#ifndef MUSIC_H
#define MUSIC_H
/*
 * apps/music.h — PC Speaker Müzik Çalar
 * =======================================
 * VFS'ten .pcm veya .notes dosyası okur ve PC Speaker'dan çalar.
 * .notes formatı: her satır "frekans süre_ms" (örn: "440 500")
 * 3 built-in melodi + VFS dosya desteği
 */
#include "../gui.h"
#include "../kernel/vfs.h"
#include "../kernel/sound.h"

/* PC Speaker frekanslar (nota adları) */
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_G5  784
#define NOTE_OFF 0

struct MusicNote { int freq; int dur_ticks; }; /* dur: PIT tick sayısı @18.2Hz */

/* Built-in melodiler */
static const MusicNote MELODY_LIGHTOS[] = {
    {NOTE_C4,4},{NOTE_E4,4},{NOTE_G4,4},{NOTE_C5,8},
    {NOTE_OFF,2},{NOTE_G4,4},{NOTE_E4,4},{NOTE_C4,8},
    {NOTE_OFF,4},{NOTE_A4,4},{NOTE_G4,4},{NOTE_F4,4},{NOTE_E4,8},
    {NOTE_C5,6},{NOTE_G4,4},{NOTE_C5,18},{NOTE_OFF,4},
    {0,0}
};
static const MusicNote MELODY_BOOT[] = {
    {NOTE_C5,3},{NOTE_G4,3},{NOTE_E4,3},{NOTE_C5,6},{NOTE_OFF,2},
    {NOTE_E5,4},{NOTE_C5,8},{NOTE_OFF,4},
    {0,0}
};
static const MusicNote MELODY_GAME[] = {
    {NOTE_E5,2},{NOTE_E5,2},{NOTE_OFF,2},{NOTE_E5,2},{NOTE_OFF,2},
    {NOTE_C5,2},{NOTE_E5,4},{NOTE_G5,8},{NOTE_OFF,4},
    {NOTE_G4,8},{NOTE_OFF,4},
    {0,0}
};

struct MusicPlayer : public Window {
    int   current_melody;  /* 0=LightOS 1=Boot 2=Game */
    int   note_idx;
    int   tick_count;
    bool  playing;
    bool  looping;
    int   vfs_node;

    /* Custom melody parsed from VFS */
    static const int CUSTOM_MAX = 64;
    MusicNote custom_melody[CUSTOM_MAX];
    int custom_len;

    MusicPlayer():Window("Music Player",100,80,320,240){
        current_melody=0; note_idx=0; tick_count=0;
        playing=false; looping=false; vfs_node=-1; custom_len=0;
        for(int i=0;i<CUSTOM_MAX;i++){custom_melody[i]={0,0};}
    }

    const MusicNote* get_melody(){
        if(current_melody==3&&custom_len>0) return custom_melody;
        if(current_melody==1) return MELODY_BOOT;
        if(current_melody==2) return MELODY_GAME;
        return MELODY_LIGHTOS;
    }

    void play(){ playing=true; note_idx=0; tick_count=0; }
    void stop(){ playing=false; spk_off(); }

    void Tick(){
        if(!playing) return;
        const MusicNote* mel=get_melody();
        if(mel[note_idx].freq==0&&mel[note_idx].dur_ticks==0){
            if(looping){ note_idx=0; tick_count=0; return; }
            stop(); return;
        }
        if(tick_count==0){
            if(mel[note_idx].freq>0) spk_on((uint32_t)mel[note_idx].freq);
            else spk_off();
        }
        tick_count++;
        if(tick_count>=mel[note_idx].dur_ticks){
            tick_count=0; note_idx++;
        }
    }

    void DrawContent() override {
        int ox=x+2,oy=y+26;
        DrawRect(ox,oy,w-4,h-28,0x0A0A14);

        /* Visualizer — basit bar animasyonu */
        if(playing){
            const MusicNote* mel=get_melody();
            int freq=mel[note_idx].freq;
            int bar_h=(freq/20)%(h-100); if(bar_h<2)bar_h=2;
            int bw=(w-24)/3;
            for(int b=0;b<3;b++){
                int bh=bar_h*(b+1)/3; if(bh<4)bh=4;
                uint32_t col=LerpColor(WP_FrameFoc(),0xFFFFFF,b*20,100);
                DrawRect(ox+8+b*(bw+4),oy+80-bh,bw,bh,col);
            }
        }

        /* Nota adı */
        DrawGradientV(ox,oy,w-4,20,LerpColor(0x0A0A14,WP_FrameFoc(),15,100),0x0A0A14);
        const char* names[]={"LightOS Theme","Boot Jingle","Game Music","Custom (.notes)"};
        DrawStringCentered(x+w/2,oy+6,names[current_melody<4?current_melody:0],WP_TitleTop(true));

        /* Kontroller */
        int cy=oy+100;

        /* << Prev */
        DrawGradientV(ox+8,cy,44,28,WP_TitleTop(true),WP_TitleBot(true));
        DrawBevel(ox+8,cy,44,28,true);
        DrawStringCentered(ox+8+22,cy+10,"<<",WP_TitleText(true));

        /* Play/Stop */
        DrawGradientV(ox+60,cy,80,28,playing?0x884400:0x224488,playing?0x441100:0x112244);
        DrawBevel(ox+60,cy,80,28,true);
        DrawStringCentered(ox+60+40,cy+10,playing?"■ Stop":"▶ Play",0xFFFFFF);

        /* >> Next */
        DrawGradientV(ox+148,cy,44,28,WP_TitleTop(true),WP_TitleBot(true));
        DrawBevel(ox+148,cy,44,28,true);
        DrawStringCentered(ox+148+22,cy+10,">>",WP_TitleText(true));

        /* Loop toggle */
        DrawGradientV(ox+200,cy,60,28,looping?0x224422:0x1A1A2A,looping?0x112211:0x0A0A14);
        DrawBevel(ox+200,cy,60,28,true);
        DrawStringCentered(ox+200+30,cy+10,looping?"LOOP ON":"LOOP",looping?0x44FF88:0x667788);

        /* Progress */
        if(playing){
            int py2=cy+38;
            DrawRect(ox+8,py2,w-20,6,0x1A1A2A);
            /* Yaklaşık ilerleme */
            const MusicNote* mel=get_melody();
            int total=0,elapsed=0;
            for(int i=0;mel[i].freq!=0||mel[i].dur_ticks!=0;i++){
                if(i<note_idx) elapsed+=mel[i].dur_ticks;
                total+=mel[i].dur_ticks;
            }
            if(total>0){
                int fill=(w-20)*elapsed/total;
                DrawRect(ox+8,py2,fill,6,WP_FrameFoc());
            }
        }

        /* Info bar */
        DrawRect(ox,oy+h-52,w-4,1,0x1A1A2A);
        DrawString(ox+8,oy+h-46,"PC Speaker · .notes = frekans ms",0x334455);
    }

    void OnClickContent(int mx,int my) override {
        int ox=x+2,oy=y+26,cy=oy+100;
        if(my>=cy&&my<=cy+28){
            if(mx>=ox+8&&mx<=ox+52){
                current_melody=(current_melody+3)%4; stop();
            }
            else if(mx>=ox+60&&mx<=ox+140){
                if(playing) stop(); else play();
            }
            else if(mx>=ox+148&&mx<=ox+192){
                current_melody=(current_melody+1)%4; stop();
            }
            else if(mx>=ox+200&&mx<=ox+260){
                looping=!looping;
            }
        }
    }
    void KeyPress(unsigned int cp){
        if(cp==' '){ if(playing) stop(); else play(); }
        if(cp=='r'){ stop(); play(); }
        if(cp=='l'){ looping=!looping; }
    }
};
#endif
