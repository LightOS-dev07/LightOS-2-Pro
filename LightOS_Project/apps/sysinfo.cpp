#include "sysinfo.h"
#include "../kernel/vfs.h"
#include "../lang.h"
#include "../kernel/clock.h"

extern uint32_t mem_upper_kb;
extern uint32_t screen_w, screen_h;

static void si_int2s(int v, char* buf){
    if(v==0){buf[0]='0';buf[1]=0;return;}
    char t[12];int i=0;
    while(v>0){t[i++]='0'+v%10;v/=10;}
    int j=0;for(int k=i-1;k>=0;k--)buf[j++]=t[k];buf[j]=0;
}
static void si_cat(char* d,const char* a,const char* b){
    int i=0;while(a[i]){d[i]=a[i];i++;}
    int j=0;while(b[j]){d[i+j]=b[j];j++;}d[i+j]=0;
}

SysInfo::SysInfo():Window("System Information",60,40,420,340){}

void SysInfo::DrawBar(int bx,int by,int bw,int bh,int pct,uint32_t col){
    DrawRect(bx,by,bw,bh,0x202020);
    DrawRectBorder(bx,by,bw,bh,0x505050);
    int filled=bw*pct/100; if(filled>bw)filled=bw;
    DrawGradientH(bx,by,filled,bh,col,BrightColor(col,130));
}

void SysInfo::DrawContent(){
    DrawGradientV(x+2,y+26,w-4,h-28,0x0C0C18,0x181828);

    /* ─ Başlık ─ */
    DrawString(x+12,y+34,"LightOS 2 Pro — System Information",0x80C0FF);
    DrawRect(x+8,y+47,w-16,1,0x304060);

    int lx=x+14,ly=y+54,lh=18;
    auto row=[&](const char* label,const char* val,uint32_t vc=0xDDEEFF){
        DrawString(lx,ly,label,0x7090B0);
        DrawString(lx+140,ly,val,vc);
        ly+=lh;
    };

    /* CPU */
    row("CPU:","AMD/Intel x86 32-bit",0xAAFFAA);
    row("Extensions:","SSE2  FPU  MTRR",0xAAFFAA);

    /* RAM */
    char ram_s[20]; si_int2s((int)(mem_upper_kb/1024),ram_s);
    char ram_full[24]; si_cat(ram_full,ram_s," MB Total");
    row("RAM:",ram_full,0xAAFFAA);

    /* RAM bar */
    ly+=2;
    int used_pct=60; /* simüle */
    DrawString(lx,ly,"Usage:",0x7090B0);
    DrawBar(lx+140,ly,200,12,used_pct,0x2288AA);
    char pct_s[8]; si_int2s(used_pct,pct_s);
    char pct_full[12]; si_cat(pct_full,pct_s,"%");
    DrawString(lx+348,ly,pct_full,0xDDEEFF);
    ly+=20;

    /* Video */
    char res_s[20];
    char ws[8],hs[8];
    si_int2s((int)screen_w,ws); si_int2s((int)screen_h,hs);
    int p=0;
    while(ws[p]) res_s[p]=ws[p++];
    res_s[p++]='x';
    int q=0; while(hs[q]) res_s[p++]=hs[q++];
    res_s[p++]=' '; res_s[p++]='3'; res_s[p++]='2'; res_s[p++]='b'; res_s[p++]='p'; res_s[p]=0;
    row("Display:",res_s,0xAAFFAA);
    row("Framebuffer:","Linear VESA/BGA",0xDDEEFF);

    /* Disk */
    row("Storage:","LightDisk (RAM FS)",0xDDEEFF);
    int vfs_files=0;
    extern VFS g_vfs;
    for(int i=0;i<g_vfs.node_count;i++) if(g_vfs.nodes[i].used) vfs_files++;
    char fc[8]; si_int2s(vfs_files,fc);
    char ff[16]; si_cat(ff,fc," nodes");
    row("VFS Nodes:",ff,0xDDEEFF);

    /* Saat */
    DrawRect(x+8,ly+2,w-16,1,0x304060);
    ly+=8;
    RtcTime t=rtc_get();
    char time_s[12],date_s[14];
    char h2[3],m2[3],s2[3];
    rtc_2d(t.hour,h2);rtc_2d(t.min,m2);rtc_2d(t.sec,s2);
    int tp=0;
    for(int i=0;h2[i];i++)time_s[tp++]=h2[i]; time_s[tp++]=':';
    for(int i=0;m2[i];i++)time_s[tp++]=m2[i]; time_s[tp++]=':';
    for(int i=0;s2[i];i++)time_s[tp++]=s2[i]; time_s[tp]=0;

    char d2[3],mo2[3],yr_s[6];
    rtc_2d(t.day,d2); rtc_2d(t.month,mo2);
    si_int2s(t.year,yr_s);
    int dp=0;
    for(int i=0;d2[i];i++)date_s[dp++]=d2[i]; date_s[dp++]='.';
    for(int i=0;mo2[i];i++)date_s[dp++]=mo2[i]; date_s[dp++]='.';
    for(int i=0;yr_s[i];i++)date_s[dp++]=yr_s[i]; date_s[dp]=0;

    row("System Time:",time_s,0xFFCC44);
    row("System Date:",date_s,0xFFCC44);

    /* OS bilgisi */
    DrawRect(x+8,ly+2,w-16,1,0x304060); ly+=8;
    row("OS:","LightOS 2 Pro",0xFFAA44);
    row("Version:","2.0.0-beta",0xDDEEFF);
    row("Author:","Xaef BTL",0xDDEEFF);
    row("License:","Proprietary",0xDDEEFF);
}

void SysInfo::UpdateLang(){ SetTitle("System Information"); }
