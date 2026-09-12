#include "filemgr.h"
#include "../lang.h"
#include "../theme.h"

static void fs_strcpy(char* d, const char* s, int n=31){
    int i=0; while(s[i]&&i<n){d[i]=s[i];i++;} d[i]=0;
}
static int fs_strlen(const char* s){ int i=0; while(s[i])i++; return i; }
static void fs_int2str(int v, char* buf){
    if(v==0){buf[0]='0';buf[1]=0;return;}
    char t[12]; int i=0;
    while(v>0){t[i++]='0'+v%10;v/=10;}
    int j=0; for(int k=i-1;k>=0;k--)buf[j++]=t[k]; buf[j]=0;
}

FileMgr::FileMgr() : Window("", 60, 40, 460, 340) {
    cwd = 0; selected = -1; scroll = 0;
    rename_mode = false; rename_cursor = 0;
    rename_buf[0] = 0; open_request_node = -1;
    SetTitle(LS(S_FILES_TITLE));
}

void FileMgr::BuildPath(int idx, char* buf, int buf_size) {
    /* kökten yola kadar geriye giderek yolu oluştur */
    if(idx < 0) { buf[0]='#'; buf[1]=0; return; }
    /* Geçici yol stacki (max 8 seviye) */
    int chain[8]; int depth=0;
    int cur = idx;
    while(cur >= 0 && depth < 8) { chain[depth++]=cur; cur=g_vfs.nodes[cur].parent; }
    /* Ters sırala ve birleştir */
    int pos=0;
    for(int i=depth-1;i>=0;i--){
        const char* n = g_vfs.nodes[chain[i]].name;
        int j=0;
        while(n[j]&&pos<buf_size-2){buf[pos++]=n[j++];}
        if(i>0&&pos<buf_size-2) buf[pos++]='/';
    }
    buf[pos]=0;
}

int FileMgr::GetChild(int nth) {
    if(cwd<0) return -1;
    VfsNode& d = g_vfs.nodes[cwd];
    int count=0;
    for(int i=0;i<d.child_count;i++){
        int ci = d.children[i];
        if(ci<0||!g_vfs.nodes[ci].used) continue;
        if(count == nth) return ci;
        count++;
    }
    return -1;
}

int visibleChildren(int cwd){
    if(cwd<0) return 0;
    VfsNode& d=g_vfs.nodes[cwd];
    int cnt=0;
    for(int i=0;i<d.child_count;i++) if(d.children[i]>=0&&g_vfs.nodes[d.children[i]].used) cnt++;
    return cnt;
}

void FileMgr::DrawFileIcon(int fx, int fy, bool is_dir, bool sel, bool hov){
    uint32_t hlcol = sel ? 0x0054E3 : (hov ? 0x80B0E0 : 0xFFFFFF);
    if(sel||hov) DrawRect(fx-2,fy-2,44,52,sel?0xCCDDFF:0xEEF4FF);
    if(is_dir){
        DrawGradientV(fx, fy+5, 40, 28, 0xFFCC00, 0xCCA000);
        DrawGradientV(fx, fy+1, 16, 6,  0xFFDD44, 0xFFCC00);
        DrawRectBorder(fx,fy+5,40,28,0xAA7700);
        DrawRect(fx+3,fy+11,34,2,0xFFE566);
    } else {
        DrawGradientV(fx, fy, 34, 42, 0xFFFFFF, 0xF0F0EC);
        DrawRect(fx+22,fy,12,12,0xD4D0C8);
        DrawRect(fx+22,fy,1,12,0x909090);
        DrawRect(fx+22,fy+12,12,1,0x909090);
        DrawRectBorder(fx,fy,34,42,0xA8A4A0);
        DrawRect(fx+4,fy+15,22,1,0xC8C4C0);
        DrawRect(fx+4,fy+21,22,1,0xC8C4C0);
        DrawRect(fx+4,fy+27,16,1,0xC8C4C0);
    }
    if(sel) DrawRectBorder(fx-2,fy-2,44,52,0x0054E3);
    (void)hov; (void)hlcol;
}

void FileMgr::DrawBreadcrumb(){
    /* Adres çubuğu */
    DrawGradientV(x+2,y+26,w-4,20,0xECE9D8,0xD8D4C8);
    DrawRect(x+2,y+45,w-4,1,0xA09888);

    /* Geri butonu */
    bool canBack = (cwd>=0 && g_vfs.nodes[cwd].parent >= 0);
    DrawGradientV(x+4,y+28,28,16,
        canBack?0xD0CCB8:0xC8C4B0, canBack?0xB8B4A0:0xB0ACA0);
    DrawBevel(x+4,y+28,28,16,true);
    DrawString(x+10,y+32,"<-",canBack?0x000000:0x888880);

    /* Yol */
    char pathbuf[128];
    BuildPath(cwd, pathbuf, 128);
    DrawRect(x+36,y+29,w-72,14,0xFFFFFF);
    DrawRectBorder(x+36,y+29,w-72,14,0x888880);
    DrawString(x+40,y+31,pathbuf,0x000080);
}

void FileMgr::DrawToolbar(){
    DrawGradientV(x+2,y+46,w-4,22,0xDDD8CC,0xCCC8BC);
    DrawRect(x+2,y+67,w-4,1,0xA09888);

    /* Yeni Klasör */
    DrawGradientV(x+5,y+49,70,16,0xECE9D8,0xD4D0C8);
    DrawBevel(x+5,y+49,70,16,true);
    DrawString(x+8,y+53,"+ Folder",0x000000);

    /* Yeni Dosya */
    DrawGradientV(x+79,y+49,62,16,0xECE9D8,0xD4D0C8);
    DrawBevel(x+79,y+49,62,16,true);
    DrawString(x+82,y+53,"+ File",0x000000);

    /* Sil */
    int di = x+145;
    DrawGradientV(di,y+49,44,16,selected>=0?0xFFDDDD:0xEEE8E0,selected>=0?0xFFB8B8:0xDDD8D0);
    DrawBevel(di,y+49,44,16,true);
    DrawString(di+4,y+53,"Delete",selected>=0?0xCC0000:0x888880);
}

void FileMgr::DrawSidebar(){
    DrawGradientV(x+2,y+68,90,h-90,0xDDD8CC,0xCCC8BC);
    DrawRect(x+91,y+68,1,h-90,0xA09888);
    DrawString(x+6,y+74,"Drives",0x504840);
    DrawRect(x+4,y+85,1,h-105,0x908878);

    /* C: */
    bool cSel = (cwd==0||(cwd>0&&g_vfs.nodes[cwd].parent==0));
    DrawGradientV(x+6,y+90,78,18,cSel?TP().title_foc_top:0xECE9D8,cSel?TP().title_foc_bot:0xD4D0C8);
    if(cSel) DrawRectBorder(x+6,y+90,78,18,TP().win_frame_foc);
    DrawString(x+10,y+95,LS(S_DISK_C),cSel?TP().title_foc_text:0x000000);

    /* D: (boş) */
    DrawRect(x+6,y+110,78,18,0xE0DCD0);
    DrawString(x+10,y+115,LS(S_DISK_D_EMPTY),0x888880);
}

void FileMgr::DrawFileList(){
    int lx=x+95, ly=y+70, lw=w-97, lh=h-92;
    DrawRect(lx,ly,lw,lh,0xFFFFFF);
    DrawRectBorder(lx,ly,lw,lh,0xC0BCB8);

    /* Sütun başlıkları */
    DrawGradientV(lx+1,ly+1,lw-2,16,0xECE9D8,0xD4D0C8);
    DrawRect(lx+1,ly+16,lw-2,1,0xA09888);
    DrawString(lx+4,ly+5,"Name",0x404040);
    DrawString(lx+lw-80,ly+5,"Size",0x404040);
    DrawString(lx+lw-40,ly+5,"Type",0x404040);

    /* Dosya listesi */
    VfsNode& dir = g_vfs.nodes[cwd];
    int iy = ly+18;
    int row = 0;

    for(int i=0;i<dir.child_count;i++){
        int ci=dir.children[i];
        if(ci<0||!g_vfs.nodes[ci].used) continue;
        /* Sistem dosyalarını gizle */
        const char* nm2=g_vfs.nodes[ci].name;
        extern bool g_dev_mode;
        if(!g_dev_mode){
            bool hidden=false;
            const char* hidelist[]={"System","kernel.bin","boot.cfg",0};
            for(int hi=0;hidelist[hi];hi++){
                const char* h=hidelist[hi]; int hj=0;
                while(h[hj]&&nm2[hj]&&h[hj]==nm2[hj]) hj++;
                if(!h[hj]&&!nm2[hj]){hidden=true;break;}
            }
            if(hidden) continue;
        }
        if(row < scroll) { row++; continue; }
        if(iy+18 > ly+lh) break;

        VfsNode& node = g_vfs.nodes[ci];
        bool sel = (selected == i);
        bool is_dir = (node.type == VFS_DIR);

        /* Seçili / hover arka plan */
        if(sel) DrawGradientV(lx+1,iy,lw-2,17,0x3060B0,0x2050A0);
        else if(row%2==0) DrawRect(lx+1,iy,lw-2,17,0xF5F5F0);
        else DrawRect(lx+1,iy,lw-2,17,0xFFFFFF);

        /* Mini ikon */
        uint32_t icCol = is_dir ? 0xFFCC00 : 0xFFFFFF;
        DrawRect(lx+4,iy+3,12,12,icCol);
        if(is_dir) DrawRect(lx+4,iy+1,6,3,icCol);
        DrawRectBorder(lx+4,iy+3,12,12,is_dir?0xAA7700:0xA0A0A0);

        /* Adı (rename modunda seçili olanı farklı) */
        if(rename_mode && sel){
            DrawRect(lx+20,iy+2,lw-100,13,0xFFFFFF);
            DrawRectBorder(lx+20,iy+2,lw-100,13,0x0054E3);
            DrawString(lx+22,iy+4,rename_buf,0x000000);
            /* İmleç */
            int cpos = lx+22+rename_cursor*9;
            DrawRect(cpos,iy+3,2,10,0x0054E3);
        } else {
            DrawString(lx+20,iy+5,node.name, sel?0xFFFFFF:0x000000);
        }

        /* Boyut */
        if(!is_dir){
            char szb[16]; fs_int2str(node.size,szb);
            int sl=fs_strlen(szb);
            /* "B" ekle */
            szb[sl]='B'; szb[sl+1]=0;
            DrawString(lx+lw-80,iy+5,szb,sel?0xDDDDDD:0x505050);
        } else {
            DrawString(lx+lw-80,iy+5,"--",sel?0xDDDDDD:0x808080);
        }

        /* Tür */
        DrawString(lx+lw-40,iy+5,is_dir?"DIR":"File",sel?0xCCCCCC:0x808080);

        iy+=17; row++;
    }
}

void FileMgr::DrawStatusBar(){
    DrawGradientV(x+2,y+h-20,w-4,18,0xDDD8CC,0xCCC8BC);
    DrawRect(x+2,y+h-21,w-4,1,0xA09888);
    char buf[64];
    int cnt = visibleChildren(cwd);
    buf[0]='0'+cnt/10; buf[1]='0'+cnt%10; buf[2]=' ';
    const char* it=" items";
    int j=3; int k=0; while(it[k]) buf[j++]=it[k++];
    /* seçiliyse adı ekle */
    if(selected>=0){
        int ci=GetChild(selected);
        if(ci>=0){
            buf[j++]=' '; buf[j++]='-'; buf[j++]=' ';
            const char* nm=g_vfs.nodes[ci].name;
            int m=0; while(nm[m]&&j<60) buf[j++]=nm[m++];
        }
    }
    buf[j]=0;
    DrawString(x+6,y+h-15,buf,0x404840);

    /* Bellek göstergesi */
    extern uint32_t mem_upper_kb;
    char mb_buf[20];
    uint32_t mb = mem_upper_kb/1024;
    fs_int2str((int)mb, mb_buf);
    int ml=fs_strlen(mb_buf);
    mb_buf[ml]='M'; mb_buf[ml+1]='B'; mb_buf[ml+2]=0;
    DrawString(x+w-60,y+h-15,mb_buf,0x606858);
}

void FileMgr::DrawContent(){
    DrawBreadcrumb();
    DrawToolbar();
    DrawSidebar();
    DrawFileList();
    DrawStatusBar();
}

void FileMgr::DoBack(){
    if(cwd>=0 && g_vfs.nodes[cwd].parent>=0){
        cwd = g_vfs.nodes[cwd].parent;
        selected=-1; scroll=0;
    }
}

void FileMgr::DoNewFolder(){
    int idx = g_vfs.MkDir(cwd, "New Folder");
    (void)idx;
}

void FileMgr::DoNewFile(){
    int idx = g_vfs.MkFile(cwd, "new_file.txt","");
    (void)idx;
}

void FileMgr::DoDelete(){
    if(selected<0) return;
    int ci=GetChild(selected);
    if(ci<0) return;
    /* Node'u kaldır */
    g_vfs.nodes[ci].used=false;
    /* Parent'ın child listesinden çıkar */
    VfsNode& par=g_vfs.nodes[cwd];
    for(int i=0;i<par.child_count;i++){
        if(par.children[i]==ci){
            /* kaydır */
            for(int j=i;j<par.child_count-1;j++) par.children[j]=par.children[j+1];
            par.children[--par.child_count]=-1;
            break;
        }
    }
    selected=-1;
}

void FileMgr::OnClickContent(int mx, int my){
    /* Geri butonu */
    if(mx>=x+4&&mx<=x+32&&my>=y+29&&my<=y+44){ DoBack(); return; }

    /* Toolbar */
    if(my>=y+49&&my<=y+64){
        if(mx>=x+5&&mx<=x+74)   { DoNewFolder(); return; }
        if(mx>=x+79&&mx<=x+140) { DoNewFile();   return; }
        if(mx>=x+145&&mx<=x+188){ DoDelete();    return; }
    }

    /* Sidebar — C: kök */
    if(mx>=x+6&&mx<=x+84&&my>=y+90&&my<=y+107){ cwd=0;selected=-1;scroll=0; return; }

    /* Dosya listesi */
    int lx=x+95,ly=y+70,lw=w-97;
    if(mx<lx||mx>lx+lw||my<ly+18) return;

    int row=(my-(ly+18))/17;
    int target_i=-1,count=0;
    VfsNode& dir=g_vfs.nodes[cwd];
    extern bool g_dev_mode;
    for(int i=0;i<dir.child_count;i++){
        int ci=dir.children[i]; if(ci<0||!g_vfs.nodes[ci].used) continue;
        /* Gizli öğeleri atla (dev mode değilse) */
        if(!g_dev_mode){
            const char* nm2=g_vfs.nodes[ci].name;
            bool hidden=false;
            const char* hidelist[]={"System","kernel.bin","boot.cfg",0};
            for(int hi=0;hidelist[hi];hi++){
                const char* h=hidelist[hi]; int hj=0;
                while(h[hj]&&nm2[hj]&&h[hj]==nm2[hj]) hj++;
                if(!h[hj]&&!nm2[hj]){hidden=true;break;}
            }
            if(hidden) continue;
        }
        if(count==row+scroll){target_i=i;break;}
        count++;
    }

    if(target_i<0){ selected=-1; return; }

    if(selected==target_i){
        /* Çift tıklama simülasyonu */
        int ci=GetChild(target_i);
        if(ci>=0&&g_vfs.nodes[ci].type==VFS_DIR){
            cwd=ci; selected=-1; scroll=0;
        } else if(ci>=0&&g_vfs.nodes[ci].type==VFS_FILE){
            /* txt dosyasını notepad'de aç */
            open_request_node = ci;
        }
    } else {
        selected=target_i;
        rename_mode=false;
    }
}

void FileMgr::KeyPress(char c){
    if(rename_mode){
        if(c=='\b'){ if(rename_cursor>0) rename_buf[--rename_cursor]=0; }
        else if(c=='\n'){
            /* Adı uygula */
            if(selected>=0&&rename_cursor>0){
                int ci=GetChild(selected);
                if(ci>=0) fs_strcpy(g_vfs.nodes[ci].name,rename_buf);
            }
            rename_mode=false;
        }
        else if(rename_cursor<VFS_NAME_LEN-1){
            rename_buf[rename_cursor++]=c; rename_buf[rename_cursor]=0;
        }
    } else {
        if(c=='r'||c=='R'){
            /* F2 yok, r tuşu ile rename */
            if(selected>=0){
                int ci=GetChild(selected);
                if(ci>=0){
                    fs_strcpy(rename_buf,g_vfs.nodes[ci].name);
                    rename_cursor=fs_strlen(rename_buf);
                    rename_mode=true;
                }
            }
        }
        if(c=='\b') DoBack();
    }
}

void FileMgr::UpdateLang(){ SetTitle(LS(S_FILES_TITLE)); }
