#include "notepad.h"
#include "../lang.h"
#include "../kernel/vfs_persist.h"

Notepad::Notepad() : Window("",50,40,380,270){
    cursor_pos=0; scroll_y=0; vfs_node=-1; modified=false;
    for(int i=0;i<BUF_SIZE;i++) codepoints[i]=0;
    filename[0]=0;
    SetTitle("Notepad");
}

void Notepad::OpenVfsNode(int idx){
    vfs_node=idx;
    cursor_pos=0; scroll_y=0; modified=false;
    for(int i=0;i<BUF_SIZE;i++) codepoints[i]=0;
    if(idx<0) return;
    const char* data=g_vfs.GetData(idx);
    int i=0;
    while(data[i]&&i<BUF_SIZE-1){
        codepoints[cursor_pos++]=(unsigned char)data[i]; i++;
    }
    codepoints[cursor_pos]=0;
    cursor_pos=0;
    /* title: filename */
    const char* nm=g_vfs.nodes[idx].name;
    int j=0; while(nm[j]&&j<30){filename[j]=nm[j];j++;} filename[j]=0;
    char tbuf[48]; int k=0;
    const char* pre="Notepad - ";
    while(pre[k]) tbuf[k]=pre[k++];
    int m=0; while(filename[m]) tbuf[k++]=filename[m++];
    tbuf[k]=0;
    SetTitle(tbuf);
}

void Notepad::SaveToVfs(){
    /* vfs_node yoksa Users/Default altında yeni dosya yarat */
    if(vfs_node<0){
        /* "Users/Default" dizinini bul */
        int users = g_vfs.FindChild(0,"Users");
        int def   = (users>=0) ? g_vfs.FindChild(users,"Default") : 0;
        if(def<0) def=0;
        vfs_node = g_vfs.MkFile(def,"untitled.txt","");
        /* filename güncelle */
        const char* nm="untitled.txt";
        int j=0; while(nm[j]&&j<30){filename[j]=nm[j];j++;} filename[j]=0;
    }
    /* UTF-32 → ASCII/Latin1 basit dönüşüm */
    char buf[BUF_SIZE]; int j=0;
    for(int i=0;i<cursor_pos&&j<BUF_SIZE-1;i++){
        unsigned int cp=codepoints[i];
        if(cp<128) buf[j++]=(char)cp;
        else buf[j++]='?'; /* TR özel → ? (gelecekte UTF-8 encode) */
    }
    buf[j]=0;
    g_vfs.WriteFile(vfs_node,buf);
    if(g_disk.present) vfs_save(); /* Ctrl+S hemen diske de yazsın, sadece shutdown'a kadar beklemesin */
    modified=false;
    /* Başlıktan * kaldır */
    char tbuf[48]; int k=0;
    const char* pre="Notepad - ";
    while(pre[k]) tbuf[k]=pre[k++];
    int m=0; while(filename[m]) tbuf[k++]=filename[m++];
    tbuf[k]=0;
    SetTitle(tbuf);
}

int Notepad::CountLines(){
    int lines=1;
    for(int i=0;i<cursor_pos;i++) if(codepoints[i]=='\n') lines++;
    return lines;
}

void Notepad::GetLineCol(int& line, int& col){
    line=1; col=0;
    for(int i=0;i<cursor_pos;i++){
        if(codepoints[i]=='\n'){line++;col=0;} else col++;
    }
}

void Notepad::DrawContent(){
    /* Araç çubuğu */
    DrawGradientV(x+2,y+26,w-4,18,0xECE9D8,0xD8D4C8);
    DrawRect(x+2,y+43,w-4,1,0xA09888);

    /* Kaydet butonu */
    bool canSave=modified;
    DrawGradientV(x+5,y+29,46,14,canSave?0xCCFFCC:0xE0DDD0,canSave?0xAADDAA:0xCCC8C0);
    DrawBevel(x+5,y+29,46,14,true);
    DrawString(x+8,y+32,"Save",canSave?0x006600:0x888880);

    /* Mod göstergesi */
    if(modified) DrawString(x+w-30,y+32,"*",0xCC0000);

    /* Satır/Sütun */
    int ln,col2; GetLineCol(ln,col2);
    char lc[20]; 
    lc[0]='L'; lc[1]='n'; lc[2]=' ';
    int v=ln; char tmp[8]; int ti=0;
    while(v>0){tmp[ti++]='0'+v%10;v/=10;} if(ti==0)tmp[ti++]='1';
    int j2=3; for(int k=ti-1;k>=0;k--) lc[j2++]=tmp[k];
    lc[j2++]=':'; lc[j2++]='C'; lc[j2++]='o'; lc[j2++]='l'; lc[j2++]=' ';
    v=col2; ti=0;
    while(v>0){tmp[ti++]='0'+v%10;v/=10;} if(ti==0)tmp[ti++]='0';
    for(int k=ti-1;k>=0;k--) lc[j2++]=tmp[k];
    lc[j2]=0;
    DrawString(x+56,y+32,lc,0x505050);

    /* Yazı alanı */
    DrawRect(x+2,y+44,w-4,h-46,0xFFFFFF);
    /* Cetvel */
    DrawRect(x+2,y+44,20,h-46,0xF5F3EC);
    DrawRect(x+21,y+44,1,h-46,0xC8C4B8);

    int tx=x+27,ty=y+50;
    int max_x=x+w-6, max_y=y+h-8;
    int line_h=12;

    /* Satır numaraları + metin */
    int cur_line=1;
    /* Cetvel numaraları */
    for(int ly=ty;ly<max_y;ly+=line_h){
        if(cur_line-scroll_y>=1){
            char nb[4]; int num=cur_line;
            nb[0]=' '; nb[1]=' '; nb[2]=' '; nb[3]=0;
            if(num<10) nb[2]='0'+num;
            else if(num<100){nb[1]='0'+num/10;nb[2]='0'+num%10;}
            DrawString(x+2,ly-1,nb,0xAAA090);
        }
        cur_line++;
        if(cur_line>CountLines()+1) break;
    }

    /* Metin render */
    int cx2=tx,cy2=ty;
    int cur_draw_line=1;
    for(int i=0;i<cursor_pos;i++){
        unsigned int cp=codepoints[i];
        if(cp=='\n'){
            cx2=tx; cy2+=line_h; cur_draw_line++;
            if(cur_draw_line<=scroll_y) cy2=ty;
        } else if(cp=='\t'){
            cx2+=27;
        } else {
            if(cur_draw_line>scroll_y&&cy2+10<=max_y){
                DrawCharCP(cx2,cy2,cp,0x000000);
                cx2+=9;
                if(cx2+9>max_x){cx2=tx;cy2+=line_h;cur_draw_line++;}
            }
        }
    }
    /* İmleç */
    if(cy2+10<=max_y&&cur_draw_line>scroll_y)
        DrawRect(cx2,cy2,2,10,0x0054E3);

    /* Status bar */
    DrawGradientV(x+2,y+h-14,w-4,12,0xD8D4C8,0xC8C4B8);
    DrawString(x+6,y+h-12,
        vfs_node>=0?filename:"[unsaved]",0x505050);
}

void Notepad::OnClickContent(int mx,int my){
    /* Kaydet butonu */
    if(mx>=x+5&&mx<=x+50&&my>=y+29&&my<=y+42){
        SaveToVfs(); return;
    }
}

void Notepad::KeyPressCP(unsigned int cp){
    /* Ctrl+S = kaydet (cp 0x13 = Ctrl+S) */
    if(cp==0x13){ SaveToVfs(); return; }

    if(cp=='\b'){
        if(cursor_pos>0){ codepoints[--cursor_pos]=0; modified=true; }
    } else if(cursor_pos<BUF_SIZE-1){
        codepoints[cursor_pos++]=cp;
        codepoints[cursor_pos]=0;
        modified=true;
    }

    /* Başlığa * ekle */
    if(modified && vfs_node>=0){
        char tbuf[48]; int k=0;
        const char* pre="* Notepad - ";
        while(pre[k]) tbuf[k]=pre[k++];
        int m=0; while(filename[m]) tbuf[k++]=filename[m++];
        tbuf[k]=0;
        SetTitle(tbuf);
    }

    /* Scroll */
    int total=CountLines();
    int visible=(h-60)/12;
    if(total-scroll_y>visible) scroll_y=total-visible;
    if(scroll_y<0) scroll_y=0;
}
void Notepad::UpdateTitle(const char* t){ SetTitle(t); }
