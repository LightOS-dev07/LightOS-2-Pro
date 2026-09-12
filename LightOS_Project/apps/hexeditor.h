#ifndef HEXEDITOR_H
#define HEXEDITOR_H
/*
 * apps/hexeditor.h — VFS Hex Editör
 * ====================================
 * Seçili VFS dosyasını hex + ASCII olarak gösterir.
 * 16 byte/satır, 22 satır görünür.
 * Navigasyon: ok tuşları, PgUp/PgDn.
 * Düzenleme: bir byte'a tıkla, hex yaz.
 */
#include "../gui.h"
#include "../kernel/vfs.h"

struct HexEditor : public Window {
    int   vfs_node;      /* -1=boş */
    int   scroll;        /* byte offset (16'nın katı) */
    int   cursor;        /* seçili byte indeksi */
    bool  edit_mode;     /* hex düzenleme aktif mi? */
    char  edit_buf[3];   /* 2 hex karakter tamponu */
    int   edit_len;

    static const int BYTES_PER_ROW = 16;
    static const int VISIBLE_ROWS  = 18;

    HexEditor():Window("Hex Editor",120,50,560,360){
        vfs_node=-1; scroll=0; cursor=0;
        edit_mode=false; edit_buf[0]=0; edit_len=0;
    }

    void open_node(int node){
        vfs_node=node; scroll=0; cursor=0;
        edit_mode=false; edit_len=0;
        if(node>=0 && g_vfs.nodes[node].used){
            char title[40]="Hex: ";
            int ti=5;
            const char* nm=g_vfs.nodes[node].name;
            while(*nm&&ti<38) title[ti++]=*nm++;
            title[ti]=0;
            SetTitle(title);
        }
    }

    int data_len(){
        if(vfs_node<0||!g_vfs.nodes[vfs_node].used) return 0;
        const char* d=g_vfs.GetData(vfs_node);
        int l=0; while(d[l]&&l<VFS_MAX_DATA) l++;
        return l;
    }

    static char hex_ch(int v){ v&=0xF; return (char)(v<10?'0'+v:'A'+v-10); }
    static int  from_hex(char c){
        if(c>='0'&&c<='9') return c-'0';
        if(c>='A'&&c<='F') return c-'A'+10;
        if(c>='a'&&c<='f') return c-'a'+10;
        return -1;
    }

    void DrawContent() override {
        int ox=x+2, oy=y+26;
        DrawRect(ox,oy,w-4,h-28,0x1A1A2E);

        if(vfs_node<0){
            DrawString(ox+10,oy+20,"No file open.",0x667788);
            DrawString(ox+10,oy+36,"Open a file from File Manager.",0x445566);
            return;
        }

        const char* d=g_vfs.GetData(vfs_node);
        int dlen=data_len();

        /* Header */
        DrawGradientV(ox,oy,w-4,18,0x0D0D1A,0x1A1A2E);
        DrawString(ox+8,oy+5,"Offset   00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII",0x44AACC);
        DrawRect(ox,oy+18,w-4,1,0x334455);

        int row_y=oy+22;
        for(int row=0;row<VISIBLE_ROWS;row++){
            int base_offset=scroll+row*BYTES_PER_ROW;
            if(base_offset>=dlen) break;

            /* Offset */
            char offs[9];
            int ov=base_offset;
            for(int i=7;i>=0;i--){ offs[i]=hex_ch(ov&0xF); ov>>=4; }
            offs[8]=0;
            DrawString(ox+6,row_y,offs,0x334466);
            DrawString(ox+6+7*9,row_y,":",0x334466);

            /* Hex bytes */
            for(int col=0;col<BYTES_PER_ROW;col++){
                int off=base_offset+col;
                int bx=ox+6+9*9+col*27+(col>=8?9:0);
                if(off<dlen){
                    uint8_t b=(uint8_t)d[off];
                    bool sel=(off==cursor);
                    if(sel) DrawRect(bx-2,row_y-1,24,14,0x2244AA);
                    char hx[3]; hx[0]=hex_ch(b>>4); hx[1]=hex_ch(b&0xF); hx[2]=0;
                    DrawString(bx,row_y,hx,sel?0xFFFFFF:(b?0xAAD0F0:0x334455));
                } else {
                    DrawString(bx,row_y,"  ",0x222233);
                }
            }

            /* ASCII */
            int ax=ox+6+9*9+BYTES_PER_ROW*27+12+9;
            for(int col=0;col<BYTES_PER_ROW;col++){
                int off=base_offset+col;
                if(off>=dlen) break;
                char ac=(char)d[off];
                bool printable=(ac>=0x20&&ac<0x7F);
                char buf[2]; buf[0]=printable?ac:'.'; buf[1]=0;
                bool sel=(off==cursor);
                DrawString(ax+col*9,row_y,buf,sel?0xFFFF44:(printable?0x88CCAA:0x334455));
            }
            row_y+=15;
        }

        /* Scrollbar */
        if(dlen>VISIBLE_ROWS*BYTES_PER_ROW){
            int sb_h=h-56, sb_x=ox+w-10;
            int max_scroll=dlen-VISIBLE_ROWS*BYTES_PER_ROW;
            int thumb_h=sb_h*VISIBLE_ROWS*BYTES_PER_ROW/dlen;
            if(thumb_h<12) thumb_h=12;
            int thumb_y=scroll*(sb_h-thumb_h)/max_scroll;
            DrawRect(sb_x,oy+20,6,sb_h,0x0D0D1A);
            DrawRect(sb_x,oy+20+thumb_y,6,thumb_h,0x44AACC);
        }

        /* Edit bar */
        int eby=y+h-20;
        DrawRect(ox,eby,w-4,1,0x334455);
        if(edit_mode){
            DrawString(ox+8,eby+4,"Edit byte: ",0x44AACC);
            DrawString(ox+90,eby+4,edit_buf,0xFFFFFF);
            DrawRect(ox+90+edit_len*9,eby+2,8,14,0x44AACC);
        } else {
            if(cursor<dlen){
                uint8_t b=(uint8_t)d[cursor];
                char info[40]; int ii=0;
                const char* pre="Byte ";
                while(*pre) info[ii++]=*pre++;
                char hx[3]; hx[0]=hex_ch(b>>4); hx[1]=hex_ch(b&0xF); hx[2]=0;
                info[ii++]='0'; info[ii++]='x'; info[ii++]=hx[0]; info[ii++]=hx[1];
                const char* mid=" = ";
                while(*mid) info[ii++]=*mid++;
                int bv=b; if(!bv){info[ii++]='0';}
                char tmp[4]; int ti=0;
                while(bv>0){tmp[ti++]='0'+bv%10;bv/=10;}
                for(int k=ti-1;k>=0;k--) info[ii++]=tmp[k];
                info[ii]=0;
                DrawString(ox+8,eby+4,info,0x667788);
            }
            DrawString(ox+w-180,eby+4,"Enter=edit  Arrows=nav",0x445566);
        }
    }

    void OnClickContent(int mx,int my) override {
        int oy=y+26+22;
        for(int row=0;row<VISIBLE_ROWS;row++){
            int row_y=oy+row*15;
            if(my<row_y||my>row_y+14) continue;
            for(int col=0;col<BYTES_PER_ROW;col++){
                int bx=x+6+9*9+col*27+(col>=8?9:0);
                if(mx>=bx-2&&mx<=bx+22){
                    cursor=scroll+row*BYTES_PER_ROW+col;
                    int dlen=data_len();
                    if(cursor>=dlen) cursor=dlen-1;
                    if(cursor<0) cursor=0;
                    return;
                }
            }
        }
    }

    void KeyPress(unsigned int cp){
        int dlen=data_len();
        if(edit_mode){
            if(cp==27){edit_mode=false;edit_len=0;edit_buf[0]=0;return;}
            if(cp=='\n'||cp=='\r'){
                if(edit_len==2){
                    int v=(from_hex(edit_buf[0])<<4)|from_hex(edit_buf[1]);
                    if(v>=0&&vfs_node>=0&&cursor<dlen)
                        g_vfs.WriteByteAt(vfs_node, cursor, (uint8_t)v);
                }
                edit_mode=false;edit_len=0;edit_buf[0]=0;
                return;
            }
            if(cp=='\b'){if(edit_len>0){edit_len--;edit_buf[edit_len]=0;}return;}
            if(edit_len<2&&from_hex((char)cp)>=0){
                edit_buf[edit_len++]=(char)cp;edit_buf[edit_len]=0;
            }
            return;
        }
        /* Navigation */
        if(cp==0xFF01&&cursor>0) cursor--;         /* left */
        if(cp==0xFF02&&cursor<dlen-1) cursor++;    /* right */
        if(cp==0xFF03&&cursor>=BYTES_PER_ROW) cursor-=BYTES_PER_ROW; /* up */
        if(cp==0xFF04&&cursor+BYTES_PER_ROW<dlen) cursor+=BYTES_PER_ROW; /* down */
        /* Scroll */
        int vis=VISIBLE_ROWS*BYTES_PER_ROW;
        if(cursor<scroll) scroll=cursor&~(BYTES_PER_ROW-1);
        if(cursor>=scroll+vis) scroll=(cursor-vis+BYTES_PER_ROW)&~(BYTES_PER_ROW-1);
        /* Edit */
        if(cp=='\n'||cp=='\r'){edit_mode=true;edit_len=0;edit_buf[0]=0;}
    }
};
#endif
