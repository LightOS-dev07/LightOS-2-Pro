#ifndef BROWSER_H
#define BROWSER_H
#include "../gui.h"
#include "../kernel/vfs.h"
#include "../kernel/clock.h"

extern bool g_dev_mode;
extern bool g_net_ready;

class Browser : public Window {
public:
    static const int URL_MAX     = 80;
    static const int HIST_MAX    = 20;
    static const int CONTENT_MAX = 4096;
    static const int LINK_MAX    = 16;
    static const int BM_MAX      = 12;

    char url_bar[URL_MAX];
    int  url_cursor;
    bool url_editing;

    char page_title[48];
    char page_content[CONTENT_MAX];
    int  scroll;

    char links[LINK_MAX][URL_MAX];
    int  link_ys[LINK_MAX];
    int  link_count;

    char history[HIST_MAX][URL_MAX];
    int  hist_pos, hist_count;

    struct { char url[64]; char name[32]; } bookmarks[BM_MAX];
    int  bm_count;

    char search_buf[48];
    int  search_len;
    bool search_mode;

    Browser();
    void DrawContent()  override;
    void OnClickContent(int mx,int my) override;
    bool HandleClick(int mx,int my,bool press) override;
    void KeyPress(unsigned int cp);
    void Navigate(const char* url);
    void GoBack();
    void GoForward();
    void AddBookmark();

private:
    void LoadBuiltin(const char* url);
    void LoadFile(const char* path);
    void PushHistory(const char* url);
    void DrawToolbar();
    void DrawPageContent();
    void DrawStatusBar();
    void DrawSearch();
    void RenderLine(const char* line, int llen, int& lx, int& ly);
    bool InSearch(const char* line);

    static int  bs(const char* s){int n=0;while(s[n])n++;return n;}
    static void bc(char* d,const char* s,int m=URL_MAX-1){
        int i=0;while(s[i]&&i<m){d[i]=s[i];i++;}d[i]=0;
    }
    static bool beq(const char* a,const char* b){
        int i=0;while(a[i]&&b[i]&&a[i]==b[i])i++;return a[i]==b[i];
    }
    static bool bsw(const char* s,const char* p){
        int i=0;while(p[i]&&s[i]==p[i])i++;return !p[i];
    }
};

Browser::Browser():Window("LightSurf",20,20,560,380){
    url_bar[0]=0; url_cursor=0; url_editing=false;
    page_title[0]=0; page_content[0]=0;
    scroll=0; link_count=0;
    hist_pos=-1; hist_count=0;
    bm_count=0;
    search_buf[0]=0; search_len=0; search_mode=false;
    for(int i=0;i<LINK_MAX;i++){links[i][0]=0;link_ys[i]=0;}
    for(int i=0;i<HIST_MAX;i++) history[i][0]=0;
    bc(bookmarks[0].url,"about:home"); bc(bookmarks[0].name,"Home");
    bc(bookmarks[1].url,"about:lightos"); bc(bookmarks[1].name,"LightOS");
    bc(bookmarks[2].url,"about:system"); bc(bookmarks[2].name,"System");
    bm_count=3;
    Navigate("about:home");
}

void Browser::PushHistory(const char* url){
    if(hist_count>0&&hist_pos>=0&&beq(history[hist_pos],url)) return;
    hist_count=hist_pos+1;
    if(hist_count>=HIST_MAX){
        for(int i=0;i<HIST_MAX-1;i++) bc(history[i],history[i+1]);
        hist_count=HIST_MAX-1;
    }
    bc(history[hist_count],url);
    hist_pos=hist_count++;
}

void Browser::LoadBuiltin(const char* url){
    if(beq(url,"about:home")){
        bc(page_title,"LightSurf — Home");
        bc(page_content,
            "#Welcome to LightSurf!\n---\n"
            "*LightOS 2 Pro* built-in browser.\n\n"
            ">Quick Links\n"
            "[link:about:lightos]About LightOS[/link]\n"
            "[link:about:system]System Info[/link]\n"
            "[link:about:help]Help[/link]\n"
            "[link:about:bookmarks]Bookmarks[/link]\n"
            "[link:file://Users/Default/readme.txt]readme.txt[/link]\n"
            "---\n"
            "*Ctrl+F* Search  *Ctrl+D* Bookmark\n"
        ,CONTENT_MAX-1);
    } else if(beq(url,"about:lightos")){
        bc(page_title,"About LightOS");
        bc(page_content,
            "#LightOS 2 Pro\n---\n"
            "*Version:* 2.0.0\n"
            "*Arch:* x86 32-bit Protected Mode\n"
            "*Author:* Xaef BTL — 2026\n"
            "---\n"
            ">Features\n"
            "- Custom GUI compositor\n"
            "- 9 built-in applications\n"
            "- VFS (RAM filesystem)\n"
            "- ACPI shutdown/restart\n"
            "- AC97 + PC Speaker audio\n"
            "- RTL8139 + Intel e1000 NIC\n"
            "- Developer Mode (F4x5)\n"
            "---\n"
            "[link:about:home]Home[/link]\n"
        ,CONTENT_MAX-1);
    } else if(beq(url,"about:system")){
        extern uint32_t mem_upper_kb,screen_w,screen_h;
        char buf[CONTENT_MAX]; int p=0;
        auto ap=[&](const char* s){while(*s&&p<CONTENT_MAX-2)buf[p++]=*s++;};
        auto ai=[&](int v){if(!v){buf[p++]='0';return;}
            char t[12];int i=0;while(v>0){t[i++]='0'+v%10;v/=10;}
            for(int k=i-1;k>=0;k--)buf[p++]=t[k];};
        ap("#System Info\n---\n");
        ap("*RAM:* "); ai((int)(mem_upper_kb/1024)); ap(" MB\n");
        ap("*Display:* "); ai((int)screen_w); ap("x"); ai((int)screen_h); ap("\n");
        RtcTime t=rtc_get();
        char h2[3],m2[3],s2[3];
        rtc_2d(t.hour,h2);rtc_2d(t.min,m2);rtc_2d(t.sec,s2);
        ap("*Time:* ");ap(h2);ap(":");ap(m2);ap(":");ap(s2);ap("\n");
        int nu=0;for(int i=0;i<g_vfs.node_count;i++)if(g_vfs.nodes[i].used)nu++;
        ap("*VFS Nodes:* ");ai(nu);ap("\n");
        ap("*Dev Mode:* ");ap(g_dev_mode?"ACTIVE":"off");ap("\n");
        ap("*Network:* ");ap(g_net_ready?"Connected":"No NIC");ap("\n");
        ap("---\n[link:about:home]Home[/link]\n");
        buf[p]=0; bc(page_title,"System Info"); bc(page_content,buf,CONTENT_MAX-1);
    } else if(beq(url,"about:bookmarks")){
        char buf[CONTENT_MAX]; int p=0;
        auto ap=[&](const char* s){while(*s&&p<CONTENT_MAX-2)buf[p++]=*s++;};
        ap("#Bookmarks\n---\n");
        for(int i=0;i<bm_count;i++){
            ap("[link:");ap(bookmarks[i].url);ap("]");
            ap(bookmarks[i].name[0]?bookmarks[i].name:bookmarks[i].url);
            ap("[/link]\n");
        }
        ap("---\n[link:about:home]Home[/link]\n");
        buf[p]=0; bc(page_title,"Bookmarks"); bc(page_content,buf,CONTENT_MAX-1);
    } else if(beq(url,"about:help")){
        bc(page_title,"Help");
        bc(page_content,
            "#LightSurf Help\n---\n"
            ">URL Types\n"
            "*about:home* — Home\n"
            "*about:system* — System info\n"
            "*file://path* — VFS file\n"
            "*http://* — Internet (needs NIC)\n\n"
            ">Markup\n"
            "*#Title* — Heading\n"
            "*---* — Divider\n"
            "*>text* — Quote\n"
            "*- item* — List\n"
            "*[link:url]text[/link]* — Link\n\n"
            ">Shortcuts\n"
            "*Ctrl+F* — Search\n"
            "*Ctrl+D* — Bookmark\n"
            "[link:about:home]Home[/link]\n"
        ,CONTENT_MAX-1);
    } else {
        bc(page_title,"Not Found");
        bc(page_content,"#Page Not Found\n\n[link:about:home]Home[/link]\n",CONTENT_MAX-1);
    }
}

void Browser::LoadFile(const char* path){
    int node=0; char seg[32]; int si=0;
    const char* p2=path;
    while(*p2){
        if(*p2=='/'){seg[si]=0;if(si>0){int c2=g_vfs.FindChild(node,seg);if(c2>=0)node=c2;else{node=-1;break;}}si=0;}
        else if(si<31) seg[si++]=*p2;
        p2++;
    }
    if(si>0){seg[si]=0;int c2=g_vfs.FindChild(node,seg);if(c2>=0)node=c2;else node=-1;}
    if(node<0||!g_vfs.nodes[node].used||g_vfs.nodes[node].type!=VFS_FILE){
        bc(page_title,"File Not Found");
        bc(page_content,"#File Not Found\n\n[link:about:home]Home[/link]\n",CONTENT_MAX-1);
        return;
    }
    bc(page_title,g_vfs.nodes[node].name);
    bc(page_content,g_vfs.GetData(node),CONTENT_MAX-1);
}

void Browser::Navigate(const char* url){
    bc(url_bar,url); url_cursor=bs(url_bar);
    url_editing=false; scroll=0; link_count=0;
    if(bsw(url,"about:"))      LoadBuiltin(url);
    else if(bsw(url,"file://")) LoadFile(url+7);
    else if(bsw(url,"http://") || bsw(url,"https://")){
        /* HTTP — ağ olmadan "No Network" sayfası göster */
        if(!g_net_ready){
            bc(page_title,"No Network");
            bc(page_content,
                "#No Network Connection\n---\n"
                "LightSurf cannot connect to the internet.\n\n"
                ">To enable networking:\n"
                "QEMU: -device e1000,netdev=n0 -netdev user,id=n0\n"
                "VBox: Intel PRO/1000 MT Server adapter\n\n"
                "[link:about:home]Home[/link]\n"
            ,CONTENT_MAX-1);
        } else {
            bc(page_title,"HTTP Not Implemented");
            bc(page_content,
                "#HTTP Coming Soon\n---\n"
                "Direct HTTP browsing is under development.\n"
                "The network stack is connected but the browser\n"
                "HTTP client is not yet stable.\n\n"
                "[link:about:home]Home[/link]\n"
            ,CONTENT_MAX-1);
        }
    } else {
        bc(page_title,"Unknown URL");
        bc(page_content,"#Unknown URL\n\nUse about: or file://\n\n[link:about:home]Home[/link]\n",CONTENT_MAX-1);
    }
    PushHistory(url);
}

void Browser::AddBookmark(){
    if(bm_count>=BM_MAX) return;
    bc(bookmarks[bm_count].url,url_bar);
    bc(bookmarks[bm_count].name,page_title,31);
    bm_count++;
}

void Browser::GoBack(){
    if(hist_pos>0){hist_pos--;Navigate(history[hist_pos]);hist_pos--;}
}
void Browser::GoForward(){
    if(hist_pos<hist_count-1){hist_pos++;Navigate(history[hist_pos]);hist_pos--;}
}
bool Browser::InSearch(const char* line){
    if(!search_len) return false;
    for(int i=0;line[i];i++){
        bool m=true;
        for(int j=0;j<search_len&&m;j++) if(line[i+j]!=search_buf[j]) m=false;
        if(m) return true;
    }
    return false;
}

void Browser::DrawToolbar(){
    int ty=y+26;
    DrawGradientV(x+2,ty,w-4,28,0xF4F2EC,0xE4E0D8);
    DrawRect(x+2,ty+27,w-4,1,0xC0BCB4);
    bool canB=(hist_pos>0),canF=(hist_pos<hist_count-1);
    /* Geri */
    DrawGradientV(x+4,ty+4,22,20,canB?0xE0DDD0:0xD0CCC4,canB?0xC8C4B8:0xBCB8B0);
    DrawBevel(x+4,ty+4,22,20,true);
    DrawString(x+9,ty+8,"<",canB?0x000000:0x888880);
    /* İleri */
    DrawGradientV(x+28,ty+4,22,20,canF?0xE0DDD0:0xD0CCC4,canF?0xC8C4B8:0xBCB8B0);
    DrawBevel(x+28,ty+4,22,20,true);
    DrawString(x+35,ty+8,">",canF?0x000000:0x888880);
    /* Yenile */
    DrawGradientV(x+52,ty+4,22,20,0xE0DDD0,0xC8C4B8);
    DrawBevel(x+52,ty+4,22,20,true); DrawString(x+57,ty+8,"R",0x000000);
    /* Ev */
    DrawGradientV(x+76,ty+4,22,20,0xE0DDD0,0xC8C4B8);
    DrawBevel(x+76,ty+4,22,20,true); DrawString(x+80,ty+8,"H",0x006600);
    /* Favori */
    DrawGradientV(x+100,ty+4,22,20,0xE0DDD0,0xC8C4B8);
    DrawBevel(x+100,ty+4,22,20,true);
    DrawString(x+104,ty+8,"*",0xCC8800);
    /* URL */
    int ux=x+126,uw=w-168;
    DrawRect(ux,ty+4,uw,20,url_editing?0xFFFFFF:0xFAF8F4);
    DrawRectBorder(ux,ty+4,uw,20,url_editing?0x4A90D8:0xA09888);
    DrawString(ux+3,ty+8,bsw(url_bar,"about:")?"@":"f",0x808888);
    DrawString(ux+14,ty+8,url_bar,url_editing?0x000080:0x404858);
    if(url_editing){int cp2=ux+14+url_cursor*9;DrawRect(cp2,ty+5,2,16,0x4A90D8);}
    /* Go */
    DrawGradientV(x+w-40,ty+4,34,20,0x4A80C0,0x2A5080);
    DrawBevel(x+w-40,ty+4,34,20,true);
    DrawString(x+w-34,ty+8,"Go",0xFFFFFF);
}

void Browser::DrawStatusBar(){
    int sy=y+h-14;
    DrawGradientV(x+2,sy,w-4,12,0xE4E0D8,0xD4D0C8);
    DrawRect(x+2,sy,w-4,1,0xC0BCB4);
    DrawString(x+6,sy+3,page_title,0x505050);
    /* Ağ durumu */
    DrawString(x+w-60,sy+3,g_net_ready?"[NET]":"[NONET]",g_net_ready?0x008800:0xAA4400);
}

void Browser::DrawSearch(){
    if(!search_mode) return;
    int sy=y+h-28;
    DrawGradientV(x+2,sy,w-4,14,0xFFFFC8,0xF0F0A0);
    DrawRectBorder(x+2,sy,w-4,14,0xC0C000);
    DrawString(x+6,sy+3,"Find: ",0x606000);
    DrawRect(x+42,sy+2,150,10,0xFFFFFF);
    DrawRectBorder(x+42,sy+2,150,10,0xC0C000);
    DrawString(x+44,sy+3,search_buf,0x000000);
}

void Browser::RenderLine(const char* line,int llen,int& lx,int& ly){
    int cx2=x+8,cw=w-20,max_y=y+h-26-(search_mode?16:4);
    if(ly>=max_y) return;
    if(!llen){ly+=6;return;}
    char lbuf[256];int li=0;
    while(li<llen&&li<255){lbuf[li]=line[li];li++;} lbuf[li]=0;
    bool ishit=search_mode&&InSearch(lbuf);
    if(ishit) DrawRect(cx2,ly-1,cw,13,0xFFFF88);
    if(lbuf[0]=='#'){
        DrawRect(cx2,ly+12,StringWidth(lbuf+1),1,WP_FrameFoc());
        DrawString(cx2,ly,lbuf+1,WP_TitleTop(true)); ly+=16;
    } else if(lbuf[0]=='-'&&lbuf[1]=='-'&&lbuf[2]=='-'){
        DrawRect(cx2,ly+4,cw,1,0xCCC8C0); ly+=10;
    } else if(lbuf[0]=='>'){
        DrawRect(cx2,ly,3,12,WP_FrameFoc());
        DrawRect(cx2+3,ly,cw-3,12,LerpColor(TP().win_content,WP_FrameFoc(),8,100));
        DrawString(cx2+7,ly+2,lbuf+1,LerpColor(0x000000,WP_FrameFoc(),40,100)); ly+=13;
    } else if(lbuf[0]=='-'&&lbuf[1]==' '){
        DrawRect(cx2+4,ly+5,4,4,WP_FrameFoc());
        DrawString(cx2+12,ly+1,lbuf+2,0x202020); ly+=13;
    } else {
        int px2=lx; const char* lq=lbuf;
        while(*lq){
            if(bsw(lq,"[link:")){
                lq+=6; char lurl[URL_MAX];int ui=0;
                while(*lq&&*lq!=']'&&ui<URL_MAX-1) lurl[ui++]=*lq++;
                lurl[ui]=0; if(*lq==']')lq++;
                char ltxt[64];int ti2=0;
                while(*lq&&!bsw(lq,"[/link]")&&ti2<63) ltxt[ti2++]=*lq++;
                ltxt[ti2]=0; if(bsw(lq,"[/link]"))lq+=7;
                int tw2=StringWidth(ltxt);
                DrawRect(px2-1,ly-1,tw2+6,13,LerpColor(TP().win_content,WP_FrameFoc(),12,100));
                DrawRectBorder(px2-1,ly-1,tw2+6,13,LerpColor(WP_FrameFoc(),0xFFFFFF,40,100));
                DrawString(px2+2,ly+1,ltxt,WP_FrameFoc());
                DrawRect(px2+2,ly+10,tw2,1,WP_FrameFoc());
                if(link_count<LINK_MAX){bc(links[link_count],lurl);link_ys[link_count]=ly;link_count++;}
                px2+=tw2+8;
            } else {
                if(px2+9>x+cw){px2=cx2;ly+=13;}
                if(ly<max_y) DrawCharCP(px2,ly+1,(unsigned char)*lq,0x202020);
                px2+=9; lq++;
            }
        }
        lx=cx2; ly+=13;
    }
}

void Browser::DrawPageContent(){
    int ay=y+54+2,ah=h-54-26-(search_mode?16:4);
    DrawRect(x+2,ay,w-4,ah,TP().win_content);
    DrawGradientV(x+2,ay,w-4,14,LerpColor(TP().win_content,WP_FrameFoc(),12,100),TP().win_content);
    DrawString(x+6,ay+3,page_title,WP_TitleTop(true));
    link_count=0;
    int lx=x+8,ly=ay+18-scroll*13;
    const char* p2=page_content;
    while(*p2){
        const char* ls=p2;int llen=0;
        while(*p2&&*p2!='\n'){p2++;llen++;}
        if(*p2=='\n')p2++;
        if(ly+13>ay&&ly<ay+ah) RenderLine(ls,llen,lx,ly);
        else {ly+=13;lx=x+8;}
    }
    /* Scrollbar */
    int tl=0;const char* pp=page_content;while(*pp){if(*pp=='\n')tl++;pp++;}
    int vl=ah/13;
    if(tl>vl){
        int sbh=ah-4,th2=sbh*vl/tl;if(th2<16)th2=16;
        int ty2=scroll*(sbh-th2)/(tl-vl);
        DrawRect(x+w-8,ay+2,6,sbh,0xD4D0C8);
        DrawRect(x+w-7,ay+2+ty2,4,th2,0x8090A8);
    }
}

void Browser::DrawContent(){
    DrawToolbar();
    DrawPageContent();
    DrawStatusBar();
    DrawSearch();
}

bool Browser::HandleClick(int mx,int my,bool press){
    if(!visible||minimized) return false;
    if(my>=y&&my<=y+25) return Window::HandleClick(mx,my,press);
    if(mx<x||mx>x+w||my<y||my>y+h) return false;
    if(press){
        int ty=y+26;
        if(my>=ty&&my<=ty+28){
            if(mx>=x+4&&mx<=x+25){GoBack();return true;}
            if(mx>=x+28&&mx<=x+49){GoForward();return true;}
            if(mx>=x+52&&mx<=x+73){Navigate(url_bar);return true;}
            if(mx>=x+76&&mx<=x+97){Navigate("about:home");return true;}
            if(mx>=x+100&&mx<=x+121){Navigate("about:bookmarks");return true;}
            if(mx>=x+126&&mx<=x+126+w-168){url_editing=true;url_cursor=bs(url_bar);return true;}
            if(mx>=x+w-40){Navigate(url_bar);return true;}
        }
        int ay=y+54+2;
        if(my>=ay&&my<=y+h-26){
            if(mx>=x+w-10){scroll+=(my<ay+(h-80)/2)?-3:3;if(scroll<0)scroll=0;return true;}
            for(int i=0;i<link_count;i++){
                if(my>=link_ys[i]&&my<=link_ys[i]+13){Navigate(links[i]);return true;}
            }
        }
    }
    return (mx>=x&&mx<=x+w&&my>=y&&my<=y+h);
}

void Browser::OnClickContent(int mx,int my){(void)mx;(void)my;}

void Browser::KeyPress(unsigned int cp){
    if(search_mode){
        if(cp==27){search_mode=false;search_buf[0]=0;search_len=0;}
        else if(cp=='\b'){if(search_len>0)search_buf[--search_len]=0;}
        else if(cp>=' '&&cp<127&&search_len<47){search_buf[search_len++]=(char)cp;search_buf[search_len]=0;}
        return;
    }
    if(!url_editing){
        if(cp==0x06){search_mode=true;search_buf[0]=0;search_len=0;}
        if(cp==0x04){AddBookmark();}
        return;
    }
    if(cp=='\n'||cp=='\r'){Navigate(url_bar);url_editing=false;}
    else if(cp==27){url_editing=false;}
    else if(cp=='\b'){if(url_cursor>0){
        int len=bs(url_bar);
        for(int i=url_cursor-1;i<len;i++)url_bar[i]=url_bar[i+1];
        url_cursor--;
    }}
    else if(cp>=' '&&cp<127&&url_cursor<URL_MAX-2){
        int len=bs(url_bar);
        for(int i=len;i>=url_cursor;i--)url_bar[i+1]=url_bar[i];
        url_bar[url_cursor++]=(char)cp;
    }
}
#endif
