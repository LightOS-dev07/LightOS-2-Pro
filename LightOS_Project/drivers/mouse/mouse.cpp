#include "mouse.h"
#include "../../shared.h"

Mouse::Mouse() {
    x=SW/2; y=SH/2;
    left=right=middle=has_moved=false;
    cycle=0; packet[0]=packet[1]=packet[2]=0;
}

void Mouse::Feed(uint8_t data) {
    switch(cycle) {
        case 0:
            if(!(data & 0x08)) return;
            packet[0]=data; cycle=1; break;
        case 1:
            packet[1]=data; cycle=2; break;
        case 2: {
            packet[2]=data; cycle=0;
            if(packet[0] & 0xC0) return; /* overflow */
            int dx=(int)(uint8_t)packet[1];
            int dy=(int)(uint8_t)packet[2];
            if(packet[0]&0x10) dx|=~0xFF;
            if(packet[0]&0x20) dy|=~0xFF;
            if(dx> SENS_MAX) dx= SENS_MAX;
            if(dx<-SENS_MAX) dx=-SENS_MAX;
            if(dy> SENS_MAX) dy= SENS_MAX;
            if(dy<-SENS_MAX) dy=-SENS_MAX;
            x+=dx; y-=dy;
            if(x<0)        x=0;
            if(y<0)        y=0;
            if(x>=(int)SW) x=SW-1;
            if(y>=(int)SH) y=SH-1;
            left  =(packet[0]&0x01)!=0;
            right =(packet[0]&0x02)!=0;
            middle=(packet[0]&0x04)!=0;
            has_moved=true;
            break;
        }
    }
}
