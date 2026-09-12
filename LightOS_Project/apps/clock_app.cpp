#include "clock_app.h"
#include "../kernel/clock.h"

/* Basit sin/cos approximation (lookup table — no libm) */
static const int SIN_TABLE[60]={
     0, 10, 21, 31, 41, 50, 59, 67, 74, 81,
    86, 91, 95, 98,100,100,100, 98, 95, 91,
    86, 81, 74, 67, 59, 50, 41, 31, 21, 10,
     0,-10,-21,-31,-41,-50,-59,-67,-74,-81,
   -86,-91,-95,-98,-100,-100,-100,-98,-95,-91,
   -86,-81,-74,-67,-59,-50,-41,-31,-21,-10
};
static int isin(int deg){ return SIN_TABLE[((deg%360)+360)%360*60/360]; }
static int icos(int deg){ return -SIN_TABLE[((deg-90+360)%360)*60/360]; }

/* Çizgi çiz (basit Bresenham) */
static void DrawLine(int x0,int y0,int x1,int y1,uint32_t col){
    int dx=x1-x0,dy=y1-y0;
    int steps=dx<0?-dx:dx; if((dy<0?-dy:dy)>steps) steps=(dy<0?-dy:dy);
    if(steps==0){PutPixel(x0,y0,col);return;}
    for(int i=0;i<=steps;i++){
        int px2=x0+dx*i/steps, py2=y0+dy*i/steps;
        PutPixel(px2,py2,col);
        PutPixel(px2+1,py2,col);
    }
}

ClockApp::ClockApp():Window("Clock",280,160,220,240){}

void ClockApp::DrawAnalog(int cx,int cy,int r){
    /* Kadran */
    for(int d=0;d<360;d+=6){
        int ox=cx+r*isin(d)/100, oy=cy+r*icos(d)/100;
        int ix=cx+(r-6)*isin(d)/100, iy=cy+(r-6)*icos(d)/100;
        DrawLine(ox,oy,ix,iy,0x404858);
    }
    /* Saat işaretleri */
    for(int h=0;h<12;h++){
        int deg=h*30;
        int ox=cx+r*isin(deg)/100, oy=cy+r*icos(deg)/100;
        int ix=cx+(r-10)*isin(deg)/100, iy=cy+(r-10)*icos(deg)/100;
        DrawLine(ox,oy,ix,iy,0xAABBCC);
        /* Saat numarası */
        int num=h==0?12:h;
        int nx=cx+(r-18)*isin(deg)/100-4;
        int ny=cy+(r-18)*icos(deg)/100-4;
        char ns[3]; ns[0]=(num>=10)?'0'+num/10:' '; ns[1]='0'+num%10; ns[2]=0;
        DrawString(nx,ny,ns,0xCCDDEE);
    }
    /* Çevre */
    for(int d=0;d<360;d+=2){
        int px2=cx+r*isin(d)/100, py2=cy+r*icos(d)/100;
        PutPixel(px2,py2,0x6090B0);
    }
}

void ClockApp::DrawContent(){
    DrawGradientV(x+2,y+26,w-4,h-28,0x08101C,0x101828);

    int cx=x+w/2, cy=y+26+(h-26)/2-10;
    int r=70;

    DrawAnalog(cx,cy,r);

    RtcTime t=rtc_get();

    /* Saniye ibresi */
    int sec_deg=t.sec*6;
    DrawLine(cx,cy,cx+(r-8)*isin(sec_deg)/100,cy+(r-8)*icos(sec_deg)/100,0xFF4444);

    /* Dakika ibresi */
    int min_deg=t.min*6 + t.sec/10;
    for(int thick=-1;thick<=1;thick++)
        DrawLine(cx+thick,cy,cx+(r-16)*isin(min_deg)/100+thick,
                 cy+(r-16)*icos(min_deg)/100,0xDDEEFF);

    /* Saat ibresi */
    int hr_deg=(t.hour%12)*30 + t.min/2;
    for(int thick=-2;thick<=2;thick++)
        DrawLine(cx+thick,cy,cx+(r-28)*isin(hr_deg)/100+thick,
                 cy+(r-28)*icos(hr_deg)/100,0x80C0FF);

    /* Merkez nokta */
    DrawRect(cx-3,cy-3,7,7,0xFFFFFF);
    DrawRect(cx-1,cy-1,3,3,0xFF4444);

    /* Dijital saat */
    char h2[3],m2[3],s2[3];
    rtc_2d(t.hour,h2); rtc_2d(t.min,m2); rtc_2d(t.sec,s2);
    char ds[12];
    ds[0]=h2[0];ds[1]=h2[1];ds[2]=':';ds[3]=m2[0];ds[4]=m2[1];
    ds[5]=':';ds[6]=s2[0];ds[7]=s2[1];ds[8]=0;

    /* 2x büyük dijital */
    int dx=x+w/2-36, dy=cy+r+8;
    DrawRect(dx-4,dy-2,82,18,0x000000);
    DrawRectBorder(dx-4,dy-2,82,18,0x204060);
    for(int bi=0;ds[bi];bi++){
        unsigned char uc=(unsigned char)ds[bi];
        if(uc<32||uc>127){dx+=18;continue;}
        const unsigned char* g=font_ascii[uc-32];
        for(int row=0;row<8;row++){
            unsigned char bits=g[row];
            for(int col=0;col<8;col++){
                if(bits&(0x80>>col)){
                    PutPixel(dx+col*2,dy+row*2,0x00FF88);
                    PutPixel(dx+col*2+1,dy+row*2,0x00FF88);
                    PutPixel(dx+col*2,dy+row*2+1,0x00FF88);
                    PutPixel(dx+col*2+1,dy+row*2+1,0x00FF88);
                }
            }
        }
        dx+=18;
    }
}
