#include "terminal.h"
#include "../lang.h"
#include "../kernel/clock.h"

/* ── String yardımcıları ── */
int Terminal::t_strlen(const char* s){ int n=0; while(s[n]) n++; return n; }
void Terminal::t_strcpy(char* d, const char* s){ int i=0; while(s[i]){d[i]=s[i];i++;} d[i]=0; }
int Terminal::t_strcmp(const char* a, const char* b){
    int i=0; while(a[i]&&b[i]&&a[i]==b[i]) i++;
    return a[i]-b[i];
}
const char* Terminal::t_skip(const char* s){
    while(*s&&*s!=' ') s++;
    while(*s==' ') s++;
    return s;
}
static bool t_startswith(const char* s, const char* pre){
    int i=0; while(pre[i]&&s[i]==pre[i]) i++;
    return !pre[i];
}

/* ── Init ── */
Terminal::Terminal() : Window("Terminal",30,30,520,320){
    out_rows=0; input_len=0; hist_count=0; hist_idx=-1;
    cwd_node=0; text_color=0x00FF44;
    for(int i=0;i<ROWS;i++) for(int j=0;j<=COLS;j++) output[i][j]=0;
    for(int i=0;i<HISTORY;i++) history[i][0]=0;
    input[0]=0;
    /* Açılış mesajı */
    PrintLine("LightOS 2 Pro Terminal v1.0");
    PrintLine("Type 'help' for a list of commands.");
    PrintLine("");
}

/* ── Çıktı ── */
void Terminal::ScrollUp(){
    for(int i=0;i<ROWS-1;i++) t_strcpy(output[i],output[i+1]);
    for(int j=0;j<=COLS;j++) output[ROWS-1][j]=0;
}

void Terminal::PrintLine(const char* text){
    if(out_rows>=ROWS){ ScrollUp(); out_rows=ROWS-1; }
    int i=0;
    while(text[i]&&i<COLS){ output[out_rows][i]=text[i]; i++; }
    output[out_rows][i]=0;
    out_rows++;
}

void Terminal::PrintChar(char c){
    if(out_rows==0) out_rows=1;
    int len=t_strlen(output[out_rows-1]);
    if(len<COLS){ output[out_rows-1][len]=c; output[out_rows-1][len+1]=0; }
    else { char tmp[2]={c,0}; PrintLine(tmp); }
}

/* ── Çizim ── */
void Terminal::DrawContent(){
    /* Terminal arka planı — koyu */
    DrawRect(x+2,y+26,w-4,h-28,0x0C0C0C);

    int tx=x+6, ty=y+30;
    int line_h=14;

    /* Çıktı satırları */
    for(int i=0;i<out_rows;i++){
        DrawString(tx,ty+i*line_h,output[i],text_color);
    }

    /* Giriş satırı */
    int iy=ty+out_rows*line_h;
    if(iy+line_h < y+h-4){
        /* Prompt: "#" (kök) veya "#/System" gibi — eski "C:/" (MS-DOS
         * disk harfi geleneği) yerine LightOS'un kendi kök simgesi "#"
         * kullanılıyor artık, hem burada hem CmdPwd()'de. */
        char prompt[64]; int pi=0;
        prompt[pi++]='#';
        if(cwd_node>0){
            int chain[8]; int depth=0; int cur=cwd_node;
            while(cur>0&&depth<8){chain[depth++]=cur;cur=g_vfs.nodes[cur].parent;}
            prompt[pi++]='/';
            for(int i=depth-1;i>=0;i--){
                const char* nm=g_vfs.nodes[chain[i]].name;
                int j=0; while(nm[j]&&pi<50) prompt[pi++]=nm[j++];
                if(i>0&&pi<50) prompt[pi++]='/';
            }
        }
        prompt[pi++]=' '; prompt[pi++]='>'; prompt[pi++]=' '; prompt[pi]=0;
        DrawString(tx,iy,prompt,0x44AAFF);
        int plen=StringWidth(prompt);
        /* Kullanıcı girişi */
        DrawString(tx+plen,iy,input,0xFFFFFF);
        /* İmleç */
        DrawRect(tx+plen+t_strlen(input)*9,iy,8,12,text_color);
    }
}

/* ── Komutları Çalıştır ── */
void Terminal::CmdHelp(){
    PrintLine("Commands:");
    PrintLine("  ls [dir]  - list directory");
    PrintLine("  cd <dir>  - change directory");
    PrintLine("  cat <f>   - print file");
    PrintLine("  echo <t>  - print text");
    PrintLine("  mkdir <n> - make directory");
    PrintLine("  touch <n> - create empty file");
    PrintLine("  write <f> <text> - overwrite file content");
    PrintLine("  rm <n>    - remove file/dir");
    PrintLine("  pwd       - print working dir");
    PrintLine("  clear     - clear screen");
    PrintLine("  ver       - version info");
    PrintLine("  mem       - memory info");
    PrintLine("  date      - current date/time");
    PrintLine("  whoami    - current user");
    PrintLine("  uptime    - system uptime");
    PrintLine("  history   - command history");
    PrintLine("  sysinfo   - system summary");
    PrintLine("  calc <a> <op> <b> - basic calculator");
    PrintLine("  color <hex|none> - change text color");
}

void Terminal::CmdLs(const char* arg){
    int node = cwd_node;
    if(arg&&arg[0]) node=g_vfs.FindChild(cwd_node,arg);
    if(node<0){PrintLine("ls: not found");return;}
    VfsNode& d=g_vfs.nodes[node];
    if(d.type!=VFS_DIR){PrintLine("ls: not a directory");return;}
    bool any=false;
    for(int i=0;i<d.child_count;i++){
        int ci=d.children[i];
        if(ci<0||!g_vfs.nodes[ci].used) continue;
        char line[COLS+1];
        int p=0;
        const char* nm=g_vfs.nodes[ci].name;
        if(g_vfs.nodes[ci].type==VFS_DIR){
            line[p++]='['; int j=0; while(nm[j]&&p<COLS-2)line[p++]=nm[j++]; line[p++]=']';
        } else {
            int j=0; while(nm[j]&&p<COLS-10)line[p++]=nm[j++];
            /* size */
            line[p++]=' '; line[p++]='(';
            int sz=g_vfs.nodes[ci].size;
            char sb[8]; int si=0;
            if(sz==0){sb[si++]='0';}
            while(sz>0){char tmp2[8]; int ti2=0; while(sz>0){tmp2[ti2++]='0'+sz%10;sz/=10;}
                for(int k=ti2-1;k>=0;k--)sb[si++]=tmp2[k]; break;}
            for(int k=0;k<si&&p<COLS-3;k++)line[p++]=sb[k];
            line[p++]='B'; line[p++]=')';
        }
        line[p]=0;
        PrintLine(line);
        any=true;
    }
    if(!any) PrintLine("(empty)");
}

void Terminal::CmdCd(const char* path){
    if(!path||!path[0]){cwd_node=0;return;}
    if(path[0]=='.'&&path[1]=='.'){
        int par=g_vfs.nodes[cwd_node].parent;
        if(par>=0) cwd_node=par;
        return;
    }
    int ci=g_vfs.FindChild(cwd_node,path);
    if(ci<0){PrintLine("cd: not found");return;}
    if(g_vfs.nodes[ci].type!=VFS_DIR){PrintLine("cd: not a directory");return;}
    cwd_node=ci;
}

void Terminal::CmdCat(const char* name){
    if(!name||!name[0]){PrintLine("cat: missing filename");return;}
    int ci=g_vfs.FindChild(cwd_node,name);
    if(ci<0){PrintLine("cat: not found");return;}
    if(g_vfs.nodes[ci].type==VFS_DIR){PrintLine("cat: is a directory");return;}
    const char* data=g_vfs.GetData(ci);
    /* Satır satır yazdır */
    char line[COLS+1]; int p=0;
    for(int i=0;data[i];i++){
        if(data[i]=='\n'||p>=COLS){line[p]=0;PrintLine(line);p=0;}
        else line[p++]=data[i];
    }
    if(p>0){line[p]=0;PrintLine(line);}
}

void Terminal::CmdEcho(const char* msg){ if(msg&&msg[0]) PrintLine(msg); }

void Terminal::CmdClear(){
    out_rows=0;
    for(int i=0;i<ROWS;i++) output[i][0]=0;
}

void Terminal::CmdMkdir(const char* name){
    if(!name||!name[0]){PrintLine("mkdir: missing name");return;}
    g_vfs.MkDir(cwd_node,name);
}

void Terminal::CmdRm(const char* name){
    if(!name||!name[0]){PrintLine("rm: missing name");return;}
    int ci=g_vfs.FindChild(cwd_node,name);
    if(ci<0){PrintLine("rm: not found");return;}
    g_vfs.nodes[ci].used=false;
    VfsNode& par=g_vfs.nodes[cwd_node];
    for(int i=0;i<par.child_count;i++){
        if(par.children[i]==ci){
            for(int j=i;j<par.child_count-1;j++) par.children[j]=par.children[j+1];
            par.children[--par.child_count]=-1; break;
        }
    }
    PrintLine("removed.");
}

void Terminal::CmdPwd(){
    char buf[64]; int p=0;
    buf[p++]='#';
    /* Chain oluştur */
    int chain[8]; int depth=0; int cur=cwd_node;
    while(cur>0&&depth<8){chain[depth++]=cur;cur=g_vfs.nodes[cur].parent;}
    if(depth>0) buf[p++]='/';
    for(int i=depth-1;i>=0;i--){
        const char* nm=g_vfs.nodes[chain[i]].name;
        int j=0; while(nm[j]&&p<60)buf[p++]=nm[j++];
        if(i>0) buf[p++]='/';
    }
    buf[p]=0;
    PrintLine(buf);
}

void Terminal::CmdVer(){
    PrintLine("LightOS 2 Pro - Kernel v2.0");
    PrintLine("Copyright Xaef BTL 2026");
    PrintLine("Arch: x86 Protected Mode + SSE2");
}

void Terminal::CmdMem(){
    extern uint32_t mem_upper_kb;
    char buf[48];
    buf[0]='R'; buf[1]='A'; buf[2]='M'; buf[3]=':'; buf[4]=' ';
    int mb=(int)(mem_upper_kb/1024); int p=5;
    char tmp[8]; int ti=0;
    if(mb==0){tmp[ti++]='0';}
    while(mb>0){tmp[ti++]='0'+mb%10;mb/=10;}
    for(int k=ti-1;k>=0;k--) buf[p++]=tmp[k];
    buf[p++]=' '; buf[p++]='M'; buf[p++]='B'; buf[p]=0;
    PrintLine(buf);
}

/* ── touch: boş dosya oluştur (varsa dokunulmuş sayılır, üstüne yazmaz) ── */
void Terminal::CmdTouch(const char* name){
    if(!name||!name[0]){PrintLine("touch: missing filename");return;}
    int ci=g_vfs.FindChild(cwd_node,name);
    if(ci>=0){PrintLine("touch: already exists");return;}
    if(g_vfs.MkFile(cwd_node,name,"")<0) PrintLine("touch: failed (disk full?)");
}

/* ── write <dosya> <metin...>: dosyanın içeriğini VERİLEN metinle DEĞİŞTİRİR ── */
void Terminal::CmdWrite(const char* args){
    if(!args||!args[0]){PrintLine("write: usage: write <file> <text>");return;}
    char fname[32]; int fi=0;
    while(args[fi]&&args[fi]!=' '&&fi<31){fname[fi]=args[fi];fi++;}
    fname[fi]=0;
    const char* text = t_skip(args);
    /* args'ta boşluk yoksa t_skip aynı stringin sonuna gider — bu durumda
     * dosya adı verilmiş ama metin verilmemiş demektir. */
    if(text==args || !fname[0]){PrintLine("write: usage: write <file> <text>");return;}
    int ci=g_vfs.FindChild(cwd_node,fname);
    if(ci<0) ci=g_vfs.MkFile(cwd_node,fname,"");
    if(ci<0){PrintLine("write: failed (disk full?)");return;}
    g_vfs.WriteFile(ci,text);
    PrintLine("written.");
}

/* ── date: RTC'den gerçek tarih/saat ── */
void Terminal::CmdDate(){
    RtcTime t=rtc_get();
    char h2[3],m2[3],s2[3],d2[3],mo2[3];
    rtc_2d(t.hour,h2); rtc_2d(t.min,m2); rtc_2d(t.sec,s2);
    rtc_2d(t.day,d2); rtc_2d(t.month,mo2);
    char buf[32]; int p=0;
    buf[p++]=d2[0];buf[p++]=d2[1];buf[p++]='/';
    buf[p++]=mo2[0];buf[p++]=mo2[1];buf[p++]='/';
    /* Yıl: RTC genelde 2 haneli (00-99) döner, 2000 ekleyerek göster */
    int yr = 2000+t.year%100;
    char yb[6]; int yi=0; int yv=yr;
    while(yv>0){yb[yi++]='0'+yv%10;yv/=10;}
    for(int k=yi-1;k>=0;k--) buf[p++]=yb[k];
    buf[p++]=' ';
    buf[p++]=h2[0];buf[p++]=h2[1];buf[p++]=':';
    buf[p++]=m2[0];buf[p++]=m2[1];buf[p++]=':';
    buf[p++]=s2[0];buf[p++]=s2[1];
    buf[p]=0;
    PrintLine(buf);
}

/* ── whoami: aktif kullanıcı (bkz. login ekranı) ── */
void Terminal::CmdWhoami(){
    extern char g_current_user[32];
    if(g_current_user[0]) PrintLine(g_current_user);
    else PrintLine("guest");
}

/* ── uptime: PIT tick sayacından yaklaşık çalışma süresi ── */
void Terminal::CmdUptime(){
    extern uint32_t g_pit_ticks;
    uint32_t secs = g_pit_ticks/18; /* PIT ~18.2Hz */
    uint32_t mins = secs/60; secs%=60;
    uint32_t hrs  = mins/60; mins%=60;
    char buf[48]; int p=0;
    const char* pre="Uptime: "; int pi=0; while(pre[pi]) buf[p++]=pre[pi++];
    auto put2=[&](uint32_t v){
        char t2[4]; int ti=0;
        if(v==0){t2[ti++]='0';}
        while(v>0){t2[ti++]='0'+v%10;v/=10;}
        for(int k=ti-1;k>=0;k--) buf[p++]=t2[k];
    };
    put2(hrs); buf[p++]='h'; buf[p++]=' ';
    put2(mins); buf[p++]='m'; buf[p++]=' ';
    put2(secs); buf[p++]='s'; buf[p]=0;
    PrintLine(buf);
}

/* ── history: komut geçmişini listele ── */
void Terminal::CmdHistory(){
    for(int i=0;i<hist_count;i++){
        char line[8]; int li=0;
        int n=i+1; char tmp3[4]; int ti3=0;
        while(n>0){tmp3[ti3++]='0'+n%10;n/=10;} if(ti3==0)tmp3[ti3++]='0';
        for(int k=ti3-1;k>=0;k--) line[li++]=tmp3[k];
        line[li++]=':'; line[li++]=' '; line[li]=0;
        char full[136]; int fi=0;
        int j=0; while(line[j]) full[fi++]=line[j++];
        j=0; while(history[i][j]&&fi<134) full[fi++]=history[i][j++];
        full[fi]=0;
        PrintLine(full);
    }
}

/* ── sysinfo: kısa sistem özeti (neofetch tarzı) ── */
void Terminal::CmdSysinfo(){
    extern uint32_t mem_upper_kb;
    extern bool g_dev_mode;
    PrintLine("LightOS 2 Pro");
    PrintLine("------------------------");
    char buf[48]; int p=0;
    const char* pre="RAM: "; int pi=0; while(pre[pi]) buf[p++]=pre[pi++];
    int mb=(int)(mem_upper_kb/1024);
    char tmp4[8]; int ti4=0;
    if(mb==0) tmp4[ti4++]='0';
    while(mb>0){tmp4[ti4++]='0'+mb%10;mb/=10;}
    for(int k=ti4-1;k>=0;k--) buf[p++]=tmp4[k];
    buf[p++]='M';buf[p++]='B';buf[p]=0;
    PrintLine(buf);
    PrintLine("Arch: x86_64 Long Mode");
    PrintLine(g_dev_mode ? "Mode: Developer" : "Mode: Normal");
}

/* ── calc: basit tam sayı hesap makinesi (+,-,*,/) ── */
void Terminal::CmdCalc(const char* expr){
    if(!expr||!expr[0]){PrintLine("calc: usage: calc <a> <op> <b>, e.g. calc 5 + 3");return;}
    long a=0; bool neg=false; int i=0;
    if(expr[i]=='-'){neg=true;i++;}
    while(expr[i]>='0'&&expr[i]<='9'){a=a*10+(expr[i]-'0');i++;}
    if(neg) a=-a;
    while(expr[i]==' ') i++;
    char op=expr[i]; if(op) i++;
    while(expr[i]==' ') i++;
    long b=0; neg=false;
    if(expr[i]=='-'){neg=true;i++;}
    while(expr[i]>='0'&&expr[i]<='9'){b=b*10+(expr[i]-'0');i++;}
    if(neg) b=-b;
    long r=0; bool ok=true;
    if(op=='+') r=a+b;
    else if(op=='-') r=a-b;
    else if(op=='*') r=a*b;
    else if(op=='/'){ if(b==0){PrintLine("calc: division by zero");return;} r=a/b; }
    else ok=false;
    if(!ok){PrintLine("calc: unknown operator (use + - * /)");return;}
    char buf[24]; int p=0;
    if(r<0){buf[p++]='-'; r=-r;}
    char tmp5[16]; int ti5=0;
    if(r==0) tmp5[ti5++]='0';
    while(r>0){tmp5[ti5++]='0'+(int)(r%10);r/=10;}
    for(int k=ti5-1;k>=0;k--) buf[p++]=tmp5[k];
    buf[p]=0;
    PrintLine(buf);
}

/* ── color <hex>: çıktı metninin rengini değiştir, örn: color 44AAFF ── */
void Terminal::CmdColor(const char* arg){
    if(!arg||!arg[0]){
        text_color=0x00FF44; /* varsayılan yeşile dön */
        PrintLine("color reset to default.");
        return;
    }
    uint32_t v=0;
    for(int i=0;arg[i]&&i<6;i++){
        char c=arg[i]; uint32_t d;
        if(c>='0'&&c<='9') d=(uint32_t)(c-'0');
        else if(c>='a'&&c<='f') d=(uint32_t)(c-'a'+10);
        else if(c>='A'&&c<='F') d=(uint32_t)(c-'A'+10);
        else { PrintLine("color: expected hex, e.g. color 44AAFF"); return; }
        v=v*16+d;
    }
    text_color=v;
    PrintLine("color changed.");
}

void Terminal::RunCommand(const char* cmd){
    /* Geçmişe ekle */
    if(hist_count<HISTORY) t_strcpy(history[hist_count++],cmd);
    hist_idx=-1;

    /* Echo komutu */
    char prompt_line[COLS+1]; int pp=0;
    prompt_line[pp++]='>';prompt_line[pp++]=' ';
    const char* c=cmd; while(*c&&pp<COLS-1)prompt_line[pp++]=*c++;
    prompt_line[pp]=0; PrintLine(prompt_line);

    /* Parse */
    if(t_strcmp(cmd,"help")==0)   CmdHelp();
    else if(t_strcmp(cmd,"clear")==0) CmdClear();
    else if(t_strcmp(cmd,"ver")==0)   CmdVer();
    else if(t_strcmp(cmd,"mem")==0)   CmdMem();
    else if(t_strcmp(cmd,"pwd")==0)   CmdPwd();
    else if(t_strcmp(cmd,"date")==0)    CmdDate();
    else if(t_strcmp(cmd,"whoami")==0)  CmdWhoami();
    else if(t_strcmp(cmd,"uptime")==0)  CmdUptime();
    else if(t_strcmp(cmd,"history")==0) CmdHistory();
    else if(t_strcmp(cmd,"sysinfo")==0) CmdSysinfo();
    else if(t_startswith(cmd,"ls"))   CmdLs(t_skip(cmd));
    else if(t_startswith(cmd,"cd"))   CmdCd(t_skip(cmd));
    else if(t_startswith(cmd,"cat"))  CmdCat(t_skip(cmd));
    else if(t_startswith(cmd,"echo")) CmdEcho(t_skip(cmd));
    else if(t_startswith(cmd,"mkdir"))CmdMkdir(t_skip(cmd));
    else if(t_startswith(cmd,"touch"))CmdTouch(t_skip(cmd));
    else if(t_startswith(cmd,"write"))CmdWrite(t_skip(cmd));
    else if(t_startswith(cmd,"calc")) CmdCalc(t_skip(cmd));
    else if(t_startswith(cmd,"color"))CmdColor(t_skip(cmd));
    else if(t_startswith(cmd,"rm"))   CmdRm(t_skip(cmd));
    else if(cmd[0]){
        /* Bilinmiyor */
        char err[COLS+1]; int ep=0;
        const char* pre="Unknown command: ";
        while(pre[ep]&&ep<COLS-20)err[ep]=pre[ep++];
        const char* cc=cmd; while(*cc&&ep<COLS-1)err[ep++]=*cc++;
        err[ep]=0; PrintLine(err);
    }
    PrintLine("");
}

void Terminal::KeyPress(unsigned int cp){
    if(cp=='\n'||cp=='\r'){
        RunCommand(input);
        input[0]=0; input_len=0;
    } else if(cp=='\b'){
        if(input_len>0){ input[--input_len]=0; }
    } else if(cp==0xFF01){ /* up arrow — geçmiş */
        if(hist_count>0){
            if(hist_idx<0) hist_idx=hist_count-1;
            else if(hist_idx>0) hist_idx--;
            t_strcpy(input,history[hist_idx]);
            input_len=t_strlen(input);
        }
    } else if(cp==0xFF02){ /* down arrow */
        if(hist_idx>=0&&hist_idx<hist_count-1){
            hist_idx++;
            t_strcpy(input,history[hist_idx]);
            input_len=t_strlen(input);
        } else { input[0]=0; input_len=0; hist_idx=-1; }
    } else if(cp>=32&&cp<127&&input_len<126){
        input[input_len++]=(char)cp;
        input[input_len]=0;
    }
}

void Terminal::UpdateLang(){ SetTitle("Terminal"); }

void Terminal::OnClickContent(int mx, int my){ (void)mx; (void)my; }
