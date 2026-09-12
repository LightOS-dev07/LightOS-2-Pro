#include "settings.h"
#include "../drivers/keyboard/keyboard.h"
#include "../kernel/wallpaper_ppm.h"

extern int g_kbd_layout;
extern int g_wallpaper;
extern LangID g_lang;
extern ThemeID g_theme;

Settings::Settings() : Window("",140,60,400,360){
    active_tab=0; kbd_selection=g_kbd_layout;
    theme_selection=(int)g_theme; wallpaper_selection=g_wallpaper; apply_pressed=false;
    UpdateLang();
}

void Settings::DrawTab(int tx,int ty,int tw,int th,const char* lbl,bool active){
    DrawGradientV(tx,ty,tw,th,active?TP().win_content:0xD0CCC0,active?0xECE9D8:0xC0BCB0);
    DrawRectBorder(tx,ty,tw,th,active?TP().win_frame_foc:0x909088);
    if(active) DrawRect(tx+1,ty+th-1,tw-2,1,TP().win_content);
    int tw2=StringWidth(lbl);
    DrawString(tx+(tw-tw2)/2,ty+(th-8)/2,lbl,active?TP().title_foc_top:0x404038);
}

void Settings::DrawKeyboardTab(){
    int px=x+4,py=y+64,pw=w-8,ph=h-90;
    DrawGradientV(px,py,pw,ph,TP().win_content,0xECE9D8);
    DrawRectBorder(px,py,pw,ph,TP().win_frame_foc);

    /* Başlık şeridi */
    DrawGradientV(px+1,py+1,pw-2,22,TP().title_foc_top,TP().title_foc_bot);
    DrawString(px+8,py+7,"Keyboard Layout",TP().title_foc_text);

    int ox=px+16,oy=py+30;

    /* TR-Q */
    bool trSel=(kbd_selection==1);
    DrawRect(ox,oy+2,12,12,trSel?TP().title_foc_top:0xFFFFFF);
    DrawRectBorder(ox,oy+2,12,12,0x606070);
    if(trSel) DrawRect(ox+3,oy+5,6,5,0xFFFFFF);
    DrawString(ox+16,oy+3,"Turkish (TR-Q)",0x000000);
    if(trSel){
        DrawString(ox+16,oy+17,"  q w e r t y u \xC4\xB1 o p \xC4\x9F \xC3\xBC",0x204880);
        DrawString(ox+16,oy+29,"  a s d f g h j k l \xC5\x9F",0x204880);
        DrawString(ox+16,oy+41,"  z x c v b n m \xC3\xB6 \xC3\xA7",0x204880);
    }

    /* EN-US */
    oy=py+100;
    bool enSel=(kbd_selection==0);
    DrawRect(ox,oy+2,12,12,enSel?TP().title_foc_top:0xFFFFFF);
    DrawRectBorder(ox,oy+2,12,12,0x606070);
    if(enSel) DrawRect(ox+3,oy+5,6,5,0xFFFFFF);
    DrawString(ox+16,oy+3,"English (EN-US)",0x000000);
    if(enSel){
        DrawString(ox+16,oy+17,"  q w e r t y u i o p [ ]",0x204880);
        DrawString(ox+16,oy+29,"  a s d f g h j k l ; '",0x204880);
        DrawString(ox+16,oy+41,"  z x c v b n m , . /",0x204880);
    }

    /* Uygula */
    bool changed=(kbd_selection!=g_kbd_layout);
    uint32_t bc=changed?0x228822:0x888888;
    DrawButton3D(x+w-110,y+h-32,100,24,BrightColor(bc,110),DimColor(bc,88),
        "Apply",0xFFFFFF);
    if(!changed)
        DrawString(px+8,y+h-26,"Already applied.",0x606060);
}

void Settings::DrawThemeTab(){
    int px=x+4,py=y+64,pw=w-8,ph=h-90;
    DrawGradientV(px,py,pw,ph,TP().win_content,0xECE9D8);
    DrawRectBorder(px,py,pw,ph,TP().win_frame_foc);

    DrawGradientV(px+1,py+1,pw-2,22,TP().title_foc_top,TP().title_foc_bot);
    DrawString(px+8,py+7,"Visual Theme",TP().title_foc_text);

    /* Theme örnekleri */
    struct TInfo { const char* name; uint32_t top; uint32_t bot; bool ice; };
    TInfo themes[]={
        {"Luna Steel",  0x0A246A, 0x2468B0, false},
        {"ColdIceBar",  0xC0D8F0, 0xE0F0FF, true },
    };

    int oy=py+30;
    for(int i=0;i<2;i++){
        bool sel=(theme_selection==i);
        /* Seçim çemberi */
        DrawRect(x+16,oy+8,12,12,sel?TP().title_foc_top:0xFFFFFF);
        DrawRectBorder(x+16,oy+8,12,12,0x606070);
        if(sel) DrawRect(x+19,oy+11,6,5,0xFFFFFF);

        /* Tema önizleme kutusu */
        int px2=x+36,py2=oy;
        DrawGradientV(px2,py2,pw-50,52,0xC8D8E8,0xA0B8CC);     /* masaüstü */
        DrawGradientV(px2,py2,pw-50,16,themes[i].top,themes[i].bot); /* başlık */
        if(themes[i].ice)
            for(int li=2;li<14;li+=4)
                DrawRect(px2,py2+li,pw-50,2,LerpColor(themes[i].top,0xFFFFFF,35,100));
        DrawString(px2+4,py2+4,themes[i].name,
            themes[i].ice?0x1A3A5C:0xFFFFFF);
        /* Mini pencere gövdesi */
        DrawRect(px2+4,py2+17,pw-60,33,0xF0F0EC);
        DrawRectBorder(px2,py2,pw-50,52,sel?TP().win_frame_foc:0x888888);
        if(sel) DrawRectBorder(px2-1,py2-1,pw-48,54,TP().win_frame_foc);

        DrawString(px2+pw-44,oy+22,themes[i].name,0x000000);
        oy+=68;
    }

    bool changed2=(theme_selection!=(int)g_theme);
    uint32_t bc2=changed2?0x228822:0x888888;
    DrawButton3D(x+w-110,y+h-32,100,24,BrightColor(bc2,110),DimColor(bc2,88),
        "Apply",0xFFFFFF);
}

void Settings::DrawContent(){
    /* Sekmeler */
    DrawGradientV(x+2,y+26,w-4,36,0xD8D4C8,0xC8C4B8);
    DrawRect(x+2,y+62,w-4,1,TP().win_frame_foc);
    DrawTab(x+6, y+30,80,32,"Keyboard",active_tab==0);
    DrawTab(x+90,y+30,64,32,"Theme",   active_tab==1);
    DrawTab(x+158,y+30,72,32,"Wallpaper",active_tab==2);
    if(active_tab==0) DrawKeyboardTab();
    else if(active_tab==1) DrawThemeTab();
    else DrawWallpaperTab();
}

void Settings::OnClickContent(int mx,int my){
    /* Sekme */
    if(my>=y+30&&my<=y+62){
        if(mx>=x+6  &&mx<=x+86)  active_tab=0;
        if(mx>=x+90 &&mx<=x+154) active_tab=1;
        if(mx>=x+158&&mx<=x+230) active_tab=2;
        return;
    }
    int px=x+4,py=y+64;

    if(active_tab==0){
        int ox=px+16;
        if(mx>=ox&&mx<=ox+200&&my>=py+32&&my<=py+44) kbd_selection=1;
        if(mx>=ox&&mx<=ox+200&&my>=py+102&&my<=py+114) kbd_selection=0;
        /* Uygula */
        if(mx>=x+w-110&&mx<=x+w-10&&my>=y+h-32&&my<=y+h-8){
            g_kbd_layout=kbd_selection;
            g_lang=(g_kbd_layout==1)?LANG_TR:LANG_EN;
        }
    } else if(active_tab==2){
        int oy2=py+32;
        for(int i=0;i<5;i++){
            if(mx>=x+16&&mx<=x+16+w-50&&my>=oy2&&my<=oy2+46){wallpaper_selection=i;}
            oy2+=46;
        }
        if(mx>=x+w-110&&mx<=x+w-10&&my>=y+h-32&&my<=y+h-8){
            extern int g_wallpaper; g_wallpaper=wallpaper_selection;
        }
    } else {
        /* Tema seçimi */
        int oy=py+30;
        for(int i=0;i<2;i++){
            if(mx>=x+16&&mx<=x+16+w-50&&my>=oy&&my<=oy+52){
                theme_selection=i;
            }
            oy+=68;
        }
        /* Uygula */
        if(mx>=x+w-110&&mx<=x+w-10&&my>=y+h-32&&my<=y+h-8){
            g_theme=(ThemeID)theme_selection;
        }
    }
}

void Settings::DrawWallpaperTab(){
    extern int g_wallpaper;
    int px=x+4,py=y+64,pw=w-8,ph=h-90;
    DrawGradientV(px,py,pw,ph,TP().win_content,0xECE9D8);
    DrawRectBorder(px,py,pw,ph,TP().win_frame_foc);
    DrawGradientV(px+1,py+1,pw-2,22,TP().title_foc_top,TP().title_foc_bot);
    DrawString(px+8,py+7,"Wallpaper",TP().title_foc_text);
    const char* names[]={"Luna Night","Dawn","Forest","Custom (photo)","Rain"};
    uint32_t cols[]={0x0A246A,0x8B1A6A,0x083018,0x404040,0x232C38};
    int oy=py+32;
    for(int i=0;i<5;i++){
        bool sel=(wallpaper_selection==i);
        DrawRect(x+16,oy+8,12,12,sel?TP().title_foc_top:0xFFFFFF);
        DrawRectBorder(x+16,oy+8,12,12,0x606070);
        if(sel) DrawRect(x+19,oy+11,6,5,0xFFFFFF);
        /* Mini önizleme */
        int px2=x+36,pw2=pw-52,ph2=36;
        DrawGradientV(px2,oy,pw2,ph2,DimColor(cols[i],60),BrightColor(cols[i],80));
        DrawRectBorder(px2,oy,pw2,ph2,sel?TP().win_frame_foc:0x888888);
        DrawString(px2+6,oy+14,names[i],0xFFFFFF);
        oy+=46;
    }
    /* Custom (fotoğraf) seçiliyken, diskte gerçekten yüklenmiş bir PPM
     * olup olmadığını göster — aksi halde kullanıcı seçip "Apply"
     * dediğinde hiçbir görünür değişiklik olmaz (kod otomatik olarak
     * Luna Night'a düşer) ve bu bir bug gibi görünebilir. */
    if(wallpaper_selection==3){
        WallpaperPPMHeader hdr;
        bool has_ppm = wp_ppm_probe(hdr);
        DrawString(x+16,oy+4, has_ppm
            ? "Photo found on disk — Apply to use it."
            : "No photo installed yet. See HOW-TO-USE.md",
            has_ppm?0x206020:0xA04040);
    }
    bool ch=(wallpaper_selection!=g_wallpaper);
    uint32_t bc=ch?0x228822:0x888888;
    DrawButton3D(x+w-110,y+h-32,100,24,BrightColor(bc,110),DimColor(bc,88),"Apply",0xFFFFFF);
}

void Settings::UpdateLang(){
    SetTitle(g_lang==LANG_TR?"Ayarlar":"Settings");
}
