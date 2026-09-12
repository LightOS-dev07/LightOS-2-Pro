#include "calculator.h"
#include "../lang.h"

static const char* BTN[4][4]={
    {"7","8","9","/"},
    {"4","5","6","*"},
    {"1","2","3","-"},
    {"C","0","=","+"}
};

void Calculator::itoa_s(int v,char* buf){
    if(v==0){buf[0]='0';buf[1]=0;return;}
    bool neg=(v<0); if(neg)v=-v;
    char tmp[12]; int i=0;
    while(v>0){tmp[i++]='0'+v%10;v/=10;}
    if(neg)tmp[i++]='-';
    int j=0; for(int k=i-1;k>=0;k--)buf[j++]=tmp[k]; buf[j]=0;
}

void Calculator::UpdateDisplay(){
    if(error){
        char e[]={'E','R','R','O','R',0};
        for(int i=0;i<6;i++) display[i]=e[i];
        return;
    }
    itoa_s(current_val, display);
}

Calculator::Calculator() : Window("",200,150,172,240){
    current_val=0;accumulator=0;last_op=0;
    new_input=true;error=false;
    display[0]='0';display[1]=0;
    SetTitle(LS(S_CALC_TITLE));
}

void Calculator::DrawButton(int bx,int by,int bw,int bh,
                            const char* label,uint32_t bg,uint32_t fg)
{
    DrawGradientV(bx,by,bw,bh,BrightColor(bg,115),DimColor(bg,85));
    DrawBevel(bx,by,bw,bh,true);
    int tw=StringWidth(label);
    DrawString(bx+(bw-tw)/2, by+(bh-8)/2, label, fg);
}

void Calculator::DrawContent(){
    /* Ekran */
    DrawGradientV(x+8,y+28,w-16,26,0x001A00,0x003000);
    DrawRectBorder(x+8,y+28,w-16,26,0x00AA00);
    /* LCD efekti - diyagonal çizgiler */
    for(int j=0;j<26;j+=2) DrawRect(x+9,y+29+j,w-18,1,0x00180000 & 0x001800);
    /* Sayı */
    int dlen=0; while(display[dlen]) dlen++;
    DrawString(x+w-16-dlen*9, y+38, display, 0x00FF44);

    /* İşlem göstergesi */
    if(last_op){
        char op[2]={last_op,0};
        DrawString(x+10, y+38, op, 0x44FF44);
    }

    /* Tuşlar */
    int bw=36,bh=32,sx=x+8,sy=y+60;
    for(int r=0;r<4;r++){
        for(int c=0;c<4;c++){
            const char* lbl=BTN[r][c];
            uint32_t bg=0x9898A8, fg=0x000000;
            if(lbl[0]=='+'||lbl[0]=='-'||lbl[0]=='*'||lbl[0]=='/')
                { bg=0x4A80C8; fg=0xFFFFFF; }
            if(lbl[0]=='=') { bg=0x228822; fg=0xFFFFFF; }
            if(lbl[0]=='C') { bg=0xCC2222; fg=0xFFFFFF; }
            DrawButton(sx+c*(bw+4), sy+r*(bh+4), bw, bh, lbl, bg, fg);
        }
    }
}

void Calculator::PressButton(int row,int col){
    const char* lbl=BTN[row][col];
    char ch=lbl[0];
    if(ch=='C'){current_val=0;accumulator=0;last_op=0;new_input=true;error=false;
        display[0]='0';display[1]=0;return;}
    if(ch>='0'&&ch<='9'){
        int d=ch-'0';
        if(new_input){current_val=d;new_input=false;}
        else if(current_val<100000000) current_val=current_val*10+d;
        UpdateDisplay();return;
    }
    if(ch=='+'||ch=='-'||ch=='*'||ch=='/'){
        if(last_op&&!new_input) PressButton(3,2); /* = önce */
        accumulator=current_val; last_op=ch; new_input=true;return;
    }
    if(ch=='='){
        if(!last_op)return;
        int a=accumulator,b=current_val,res=0; error=false;
        switch(last_op){
            case '+':res=a+b;break; case '-':res=a-b;break;
            case '*':res=a*b;break;
            case '/':if(b==0){error=true;UpdateDisplay();last_op=0;return;}
                     res=a/b;break;
        }
        current_val=res; last_op=0; new_input=true; UpdateDisplay();
    }
}

void Calculator::OnClickContent(int mx,int my){
    int bw=36,bh=32,sx=x+8,sy=y+60;
    for(int r=0;r<4;r++) for(int c=0;c<4;c++){
        int bx=sx+c*(bw+4), by=sy+r*(bh+4);
        if(mx>=bx&&mx<=bx+bw&&my>=by&&my<=by+bh){PressButton(r,c);return;}
    }
}

void Calculator::UpdateLang(){
    SetTitle(LS(S_CALC_TITLE));
}
