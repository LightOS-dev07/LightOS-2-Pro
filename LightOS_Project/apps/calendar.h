#ifndef CALENDAR_H
#define CALENDAR_H
/*
 * apps/calendar.h — Takvim Uygulaması
 * =====================================
 * Gerçek RTC tarihiyle aylık takvim.
 * Not ekleme (günlük, VFS'e kayıt).
 */
#include "../gui.h"
#include "../kernel/clock.h"
#include "../kernel/vfs.h"

static const char* CAL_MONTHS[] = {
    "","January","February","March","April","May","June",
    "July","August","September","October","November","December"
};
static const char* CAL_DAYS[] = {"Mo","Tu","We","Th","Fr","Sa","Su"};

static int cal_days_in_month(int m, int y){
    if(m==2) return (y%4==0&&(y%100!=0||y%400==0))?29:28;
    if(m==4||m==6||m==9||m==11) return 30;
    return 31;
}
/* Zeller — 0=Sun,1=Mon...6=Sat, returns Mon-based 0=Mon */
static int cal_weekday(int d, int m, int y){
    if(m<3){m+=12;y--;}
    int k=y%100,j=y/100;
    int h=(d+13*(m+1)/5+k+k/4+j/4+5*j)%7;
    /* h: 0=Sat,1=Sun,2=Mon,3=Tue,4=Wed,5=Thu,6=Fri */
    return (h+5)%7; /* 0=Mon */
}

struct CalendarApp : public Window {
    int view_month, view_year;
    int sel_day;
    char note[128];
    bool editing_note;
    int note_cursor;

    CalendarApp():Window("Calendar",60,40,380,300){
        RtcTime t=rtc_get();
        view_month=t.month?t.month:1;
        view_year=2000+t.year;
        sel_day=t.day?t.day:1;
        note[0]=0; editing_note=false; note_cursor=0;
        if(!view_year||view_year<2020) view_year=2026;
    }

    void DrawContent() override {
        DrawRect(x+2,y+26,w-4,h-28,TP().win_content);

        /* Header */
        DrawGradientV(x+2,y+26,w-4,22,WP_TitleTop(true),WP_TitleBot(true));
        /* Önceki ay */
        DrawGradientV(x+4,y+28,20,18,TP().btn_min_top,TP().btn_min_bot);
        DrawBevel(x+4,y+28,20,18,true);
        DrawString(x+8,y+32,"<",WP_TitleText(true));
        /* Sonraki ay */
        DrawGradientV(x+w-24,y+28,20,18,TP().btn_min_top,TP().btn_min_bot);
        DrawBevel(x+w-24,y+28,20,18,true);
        DrawString(x+w-20,y+32,">",WP_TitleText(true));
        /* Ay-yıl */
        char hdr[32]; int hi=0;
        const char* mn=CAL_MONTHS[view_month];
        while(*mn) hdr[hi++]=*mn++;
        hdr[hi++]=' ';
        int yr=view_year; char tmp[8]; int ti=0;
        while(yr>0){tmp[ti++]='0'+yr%10;yr/=10;} if(!ti)tmp[ti++]='0';
        for(int k=ti-1;k>=0;k--) hdr[hi++]=tmp[k];
        hdr[hi]=0;
        DrawStringCentered(x+w/2,y+32,hdr,WP_TitleText(true));

        /* Gün başlıkları */
        int gx=x+6, gy=y+50;
        int cw2=(w-12)/7;
        for(int i=0;i<7;i++){
            bool wknd=(i>=5);
            DrawRect(gx+i*cw2,gy,cw2,16,LerpColor(TP().win_content,WP_FrameFoc(),12,100));
            DrawStringCentered(gx+i*cw2+cw2/2,gy+4,CAL_DAYS[i],wknd?0xCC2200:WP_TitleTop(true));
        }

        /* Günler */
        int first=cal_weekday(1,view_month,view_year);
        int total=cal_days_in_month(view_month,view_year);
        RtcTime now=rtc_get();
        int today=(2000+now.year==view_year&&now.month==view_month)?now.day:0;
        int row2=0,col2=first;
        for(int d=1;d<=total;d++){
            int dx=gx+col2*cw2;
            int dy=gy+20+row2*26;
            bool isSel=(d==sel_day);
            bool isToday=(d==today);
            bool wknd=(col2>=5);
            if(isSel){
                DrawRect(dx,dy,cw2,22,WP_FrameFoc());
                DrawBevel(dx,dy,cw2,22,false);
            } else if(isToday){
                DrawRect(dx,dy,cw2,22,LerpColor(TP().win_content,WP_FrameFoc(),25,100));
            }
            char ds[4]; ds[0]=(d>=10)?'0'+d/10:'0'+d; ds[1]='0'+d%10; ds[2]=0;
            uint32_t tc=isSel?WP_TitleText(true):(wknd?0xCC2200:(isToday?WP_FrameFoc():0x202020));
            DrawStringCentered(dx+cw2/2,dy+6,ds,tc);
            col2++;
            if(col2==7){col2=0;row2++;}
        }

        /* Not alanı */
        int ny=y+h-46;
        DrawRect(x+2,ny,w-4,1,0xCCC8C0);
        DrawString(x+8,ny+4,"Note:",WP_TitleTop(true));
        DrawRect(x+42,ny+2,w-90,20,editing_note?0xFFFFFF:0xF8F6F2);
        DrawRectBorder(x+42,ny+2,w-90,20,editing_note?WP_FrameFoc():0xA09888);
        DrawString(x+46,ny+8,note,0x202020);
        if(editing_note){int cp2=x+46+note_cursor*9;DrawRect(cp2,ny+4,2,14,WP_FrameFoc());}
        DrawGradientV(x+w-44,ny+2,38,20,WP_TitleTop(true),WP_TitleBot(true));
        DrawBevel(x+w-44,ny+2,38,20,true);
        DrawString(x+w-36,ny+8,"Save",WP_TitleText(true));
    }

    void OnClickContent(int mx,int my) override {
        /* Header navigasyon */
        if(my>=y+28&&my<=y+46){
            if(mx>=x+4&&mx<=x+24){ prev_month(); return; }
            if(mx>=x+w-24&&mx<=x+w-4){ next_month(); return; }
        }
        /* Gün tıklama */
        int gx=x+6, gy=y+50;
        int cw2=(w-12)/7;
        int first=cal_weekday(1,view_month,view_year);
        int total=cal_days_in_month(view_month,view_year);
        int col2=first, row2=0;
        for(int d=1;d<=total;d++){
            int dx=gx+col2*cw2, dy=gy+20+row2*26;
            if(mx>=dx&&mx<=dx+cw2&&my>=dy&&my<=dy+22){ sel_day=d; return; }
            col2++; if(col2==7){col2=0;row2++;}
        }
        /* Not kaydet */
        int ny=y+h-46;
        if(mx>=x+42&&mx<=x+42+w-90&&my>=ny+2&&my<=ny+22){ editing_note=true; return; }
        if(mx>=x+w-44&&my>=ny+2&&my<=ny+22){ save_note(); return; }
        editing_note=false;
    }

    void KeyPress(unsigned int cp){
        if(!editing_note) return;
        int len=0; while(note[len])len++;
        if(cp=='\b'){if(note_cursor>0){for(int i=note_cursor-1;i<len;i++)note[i]=note[i+1];note_cursor--;}}
        else if(cp=='\n'){save_note();editing_note=false;}
        else if(cp==27){editing_note=false;}
        else if(cp>=' '&&cp<127&&note_cursor<120){
            for(int i=len;i>=note_cursor;i--)note[i+1]=note[i];
            note[note_cursor++]=(char)cp;
        }
    }

    void prev_month(){
        view_month--; if(view_month<1){view_month=12;view_year--;}
        sel_day=1;
    }
    void next_month(){
        view_month++; if(view_month>12){view_month=1;view_year++;}
        sel_day=1;
    }
    void save_note(){
        /* VFS'e kaydet: Calendar/YYYY-MM-DD.txt */
        if(!note[0]) return;
        char path[40]="Calendar";
        g_vfs.MkDir(0,path);
        int dir=g_vfs.FindChild(0,"Calendar");
        if(dir<0) return;
        char fname[20];
        /* YYYY-MM-DD */
        int yi=view_year,mi=view_month,di2=sel_day;
        fname[0]='0'+yi/1000;fname[1]='0'+(yi/100)%10;
        fname[2]='0'+(yi/10)%10;fname[3]='0'+yi%10;
        fname[4]='-';fname[5]='0'+mi/10;fname[6]='0'+mi%10;
        fname[7]='-';fname[8]='0'+di2/10;fname[9]='0'+di2%10;
        fname[10]='.';fname[11]='t';fname[12]='x';fname[13]='t';fname[14]=0;
        int node=g_vfs.FindChild(dir,fname);
        if(node<0){ node=g_vfs.MkFile(dir,fname,""); }
        if(node>=0) g_vfs.WriteFile(node,note);
    }
};
#endif
