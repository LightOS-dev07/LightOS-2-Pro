#ifndef DEVTOOLS_H
#define DEVTOOLS_H
/*
 * apps/devtools.h — LightOS Developer Tools
 * Dev Mode aktifken açılabilen 10 araç.
 * Hepsi tek header'da, hafif implementasyon.
 */
#include "../gui.h"
#include "../kernel/vfs.h"
#include "../kernel/clock.h"

extern bool g_dev_mode;

/* ── 1. Memory Viewer ── */
struct MemViewer : public Window {
    uint32_t addr;
    MemViewer():Window("MemViewer",20,20,400,260){ addr=0x00000000; }
    void DrawContent() override {
        DrawRect(x+2,y+26,w-4,h-28,0x050A0A);
        /* Adres çubuğu */
        DrawRect(x+4,y+30,w-8,14,0x0A1A1A);
        DrawRectBorder(x+4,y+30,w-8,14,0x204040);
        char ab[12]; ab[0]='0';ab[1]='x';
        const char* hx="0123456789ABCDEF";
        for(int i=0;i<8;i++) ab[2+i]=hx[(addr>>(28-i*4))&0xF]; ab[10]=0;
        DrawString(x+8,y+33,ab,0x00FF88);
        /* 16×16 hex dump */
        for(int row=0;row<12;row++){
            uint32_t ra=addr+(uint32_t)(row*16);
            /* Adres */
            char rb[10]; rb[0]='0';rb[1]='x';
            for(int i=0;i<6;i++) rb[2+i]=hx[(ra>>(20-i*4))&0xF]; rb[8]=0;
            DrawString(x+4,y+48+row*14,rb,0x4488AA);
            /* Hex baytlar */
            for(int col=0;col<8;col++){
                uint8_t* p=(uint8_t*)(uintptr_t)(ra+(uint32_t)col);
                char hb[3]; hb[0]=hx[(*p>>4)&0xF]; hb[1]=hx[*p&0xF]; hb[2]=0;
                DrawString(x+76+col*22,y+48+row*14,hb,0x00CC66);
            }
        }
        /* Kontroller */
        DrawGradientV(x+4,y+h-22,60,18,0x103030,0x081818);
        DrawBevel(x+4,y+h-22,60,18,true);
        DrawString(x+8,y+h-17,"-0x100",0x88CCAA);
        DrawGradientV(x+70,y+h-22,60,18,0x103030,0x081818);
        DrawBevel(x+70,y+h-22,60,18,true);
        DrawString(x+74,y+h-17,"+0x100",0x88CCAA);
    }
    void OnClickContent(int mx,int my) override {
        if(my>=y+h-22&&my<=y+h-4){
            if(mx>=x+4&&mx<=x+63)  { if(addr>=0x100) addr-=0x100; }
            if(mx>=x+70&&mx<=x+129){ addr+=0x100; }
        }
    }
};

/* ── 2. Port IO Tester ── */
struct PortTester : public Window {
    uint16_t port; uint8_t last_val;
    PortTester():Window("Port I/O",220,30,300,200){ port=0x60; last_val=0; }
    void DrawContent() override {
        DrawRect(x+2,y+26,w-4,h-28,0x080810);
        DrawString(x+8,y+34,"Port:",0x8899BB);
        char pb[8]; pb[0]='0';pb[1]='x';
        const char* hx="0123456789ABCDEF";
        pb[2]=hx[(port>>12)&0xF]; pb[3]=hx[(port>>8)&0xF];
        pb[4]=hx[(port>>4)&0xF]; pb[5]=hx[port&0xF]; pb[6]=0;
        DrawString(x+44,y+34,pb,0xFFCC44);
        DrawString(x+8,y+54,"Last IN:",0x8899BB);
        char vb[5]; vb[0]='0';vb[1]='x';
        vb[2]=hx[(last_val>>4)&0xF]; vb[3]=hx[last_val&0xF]; vb[4]=0;
        DrawString(x+64,y+54,vb,0x00FF88);
        /* Butonlar */
        DrawGradientV(x+8,y+76,60,20,0x102020,0x081010);
        DrawBevel(x+8,y+76,60,20,true);
        DrawString(x+14,y+81,"Read IN",0x44CCAA);
        DrawGradientV(x+76,y+76,60,20,0x201010,0x100808);
        DrawBevel(x+76,y+76,60,20,true);
        DrawString(x+82,y+81,"OUT 0",0xFF8844);
        /* Port seçiciler */
        DrawString(x+8,y+108,"Common ports:",0x556677);
        const char* ports[]{"0x60 KBD","0x64 CMD","0x70 RTC","0x80 DBG"};
        for(int i=0;i<4;i++)
            DrawString(x+8+i%2*120,y+122+i/2*14,ports[i],0x7799BB);
    }
    void OnClickContent(int mx,int my) override {
        if(my>=y+76&&my<=y+96){
            if(mx>=x+8&&mx<=x+68){
                uint8_t v;
                __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port));
                last_val=v;
            }
            if(mx>=x+76&&mx<=x+136){
                __asm__ volatile("outb %0,%1"::"a"((uint8_t)0),"Nd"(port));
            }
        }
        (void)mx;(void)my;
    }
};

/* ── 3. VFS Inspector ── */
struct VfsInspector : public Window {
    int scroll;
    VfsInspector():Window("VFS Inspector",40,50,420,280){ scroll=0; }
    void DrawContent() override {
        DrawRect(x+2,y+26,w-4,h-28,0x060810);
        DrawString(x+6,y+32,"Idx Type Size  Parent Name",0x6688AA);
        DrawRect(x+2,y+42,w-4,1,0x204060);
        int iy=y+46; int sh=(h-50)/14;
        for(int i=scroll;i<g_vfs.node_count&&iy<y+h-4;i++){
            if(!g_vfs.nodes[i].used){iy+=14;continue;}
            VfsNode& n=g_vfs.nodes[i];
            const char* hx="0123456789";
            char idx_s[4]; idx_s[0]='0'+i/10; idx_s[1]='0'+i%10; idx_s[2]=0;
            DrawString(x+6,iy,idx_s,0x8899BB);
            DrawString(x+28,iy,n.type==VFS_DIR?"DIR ":"FILE",
                n.type==VFS_DIR?0xFFCC44:0x88DDAA);
            char sz[6]; int s2=n.size,si=0;
            if(s2==0){sz[si++]='0';}
            while(s2>0){char t[6];int ti=0;while(s2>0){t[ti++]='0'+s2%10;s2/=10;}
                for(int k=ti-1;k>=0;k--)sz[si++]=t[k];break;}
            sz[si]=0;
            DrawString(x+68,iy,sz,0xCCDDEE);
            char par[4]; par[0]='0'+(n.parent<0?0:n.parent/10);
                         par[1]='0'+(n.parent<0?0:n.parent%10); par[2]=0;
            DrawString(x+108,iy,n.parent<0?"--":par,0x7788AA);
            DrawString(x+136,iy,n.name,0xEEEEFF);
            iy+=14;
        }
        DrawGradientV(x+4,y+h-22,40,18,0x102030,0x081018);
        DrawBevel(x+4,y+h-22,40,18,true);
        DrawString(x+10,y+h-17,"Up",0x88BBDD);
        DrawGradientV(x+48,y+h-22,40,18,0x102030,0x081018);
        DrawBevel(x+48,y+h-22,40,18,true);
        DrawString(x+54,y+h-17,"Down",0x88BBDD);
        DrawString(x+100,y+h-17,"Nodes used:",0x556677);
        char nc[4]; int nu=0;
        for(int i=0;i<g_vfs.node_count;i++) if(g_vfs.nodes[i].used) nu++;
        nc[0]='0'+nu/10;nc[1]='0'+nu%10;nc[2]=0;
        DrawString(x+186,y+h-17,nc,0xAADDFF);
        (void)sh;
    }
    void OnClickContent(int mx,int my) override {
        if(my>=y+h-22&&my<=y+h-4){
            if(mx>=x+4&&mx<=x+44&&scroll>0) scroll--;
            if(mx>=x+48&&mx<=x+88&&scroll<g_vfs.node_count-10) scroll++;
        }
    }
};

/* ── 4. CPU Register Viewer ── */
struct CpuInfo : public Window {
    uint32_t reg_eax,reg_ebx,reg_ecx,reg_edx;
    uint32_t reg_cr0,reg_cr3;
    CpuInfo():Window("CPU Registers",60,60,360,240){
        reg_eax=reg_ebx=reg_ecx=reg_edx=0;
        reg_cr0=reg_cr3=0;
    }
    void Capture(){
        uint64_t _t;
        __asm__ volatile("mov %%rax,%0":"=r"(_t)); reg_eax=(uint32_t)_t;
        __asm__ volatile("mov %%rbx,%0":"=r"(_t)); reg_ebx=(uint32_t)_t;
        __asm__ volatile("mov %%rcx,%0":"=r"(_t)); reg_ecx=(uint32_t)_t;
        __asm__ volatile("mov %%rdx,%0":"=r"(_t)); reg_edx=(uint32_t)_t;
        __asm__ volatile("mov %%cr0,%0":"=r"(_t)); reg_cr0=(uint32_t)_t;
        __asm__ volatile("mov %%cr3,%0":"=r"(_t)); reg_cr3=(uint32_t)_t;
    }
    void DrawReg(const char* name,uint32_t val,int rx,int ry){
        DrawString(rx,ry,name,0x6688BB);
        const char* hx="0123456789ABCDEF";
        char buf[12]; buf[0]='0';buf[1]='x';
        for(int i=0;i<8;i++) buf[2+i]=hx[(val>>(28-i*4))&0xF]; buf[10]=0;
        DrawString(rx+36,ry,buf,0x00FF88);
    }
    void DrawContent() override {
        DrawRect(x+2,y+26,w-4,h-28,0x060810);
        DrawString(x+8,y+32,"General Purpose",0x4466AA);
        DrawReg("EAX:",reg_eax,x+8, y+48);
        DrawReg("EBX:",reg_ebx,x+8, y+62);
        DrawReg("ECX:",reg_ecx,x+8, y+76);
        DrawReg("EDX:",reg_edx,x+8, y+90);
        DrawRect(x+2,y+106,w-4,1,0x203050);
        DrawString(x+8,y+112,"Control Registers",0x4466AA);
        DrawReg("CR0:",reg_cr0,x+8,y+128);
        DrawReg("CR3:",reg_cr3,x+8,y+142);
        DrawString(x+8,y+160,"CR0 flags:",0x556677);
        DrawString(x+8,y+174,reg_cr0&1?"PE=1 ":"PE=0 ",reg_cr0&1?0x44FF44:0xFF4444);
        DrawString(x+56,y+174,reg_cr0&(1<<31)?"PG=1":"PG=0",reg_cr0&(1<<31)?0x44FF44:0xFF4444);
        /* Capture butonu */
        DrawGradientV(x+8,y+h-24,80,20,0x102030,0x081018);
        DrawBevel(x+8,y+h-24,80,20,true);
        DrawString(x+14,y+h-19,"Capture",0x88CCFF);
    }
    void OnClickContent(int mx,int my) override {
        if(mx>=x+8&&mx<=x+88&&my>=y+h-24&&my<=y+h-4) Capture();
    }
};

/* ── 5. IRQ Monitor ── */
struct IrqMonitor : public Window {
    int irq_count[16];
    IrqMonitor():Window("IRQ Monitor",80,40,280,240){
        for(int i=0;i<16;i++) irq_count[i]=0;
    }
    void DrawContent() override {
        DrawRect(x+2,y+26,w-4,h-28,0x060810);
        DrawString(x+6,y+32,"IRQ  Name           Count",0x6688AA);
        DrawRect(x+2,y+42,w-4,1,0x204060);
        const char* names[]={"Timer","Keyboard","Cascade","COM2",
                             "COM1","LPT2","Floppy","LPT1",
                             "RTC","Free","Free","Free",
                             "PS2 Mouse","FPU","IDE0","IDE1"};
        for(int i=0;i<16;i++){
            int iy=y+46+i*11;
            char ib[4]; ib[0]='0'+i/10;ib[1]='0'+i%10;ib[2]=0;
            DrawString(x+6,iy,ib,0x8899BB);
            DrawString(x+26,iy,names[i],0xAABBCC);
            char cb[6]; int cv=irq_count[i],ci2=0;
            if(cv==0){cb[ci2++]='0';}
            while(cv>0){char t[6];int ti=0;while(cv>0){t[ti++]='0'+cv%10;cv/=10;}
                for(int k=ti-1;k>=0;k--)cb[ci2++]=t[k];break;}
            cb[ci2]=0;
            DrawString(x+180,iy,cb,irq_count[i]>0?0x44FF44:0x445555);
        }
    }
    void OnClickContent(int mx,int my) override { (void)mx;(void)my; }
};

/* ── 6. Heap/Stack Viewer ── */
struct StackViewer : public Window {
    StackViewer():Window("Stack View",100,50,360,240){}
    void DrawContent() override {
        DrawRect(x+2,y+26,w-4,h-28,0x060810);
        uint64_t _rsp;
        __asm__ volatile("mov %%rsp,%0":"=r"(_rsp));
        uint32_t esp_val=(uint32_t)_rsp;
        DrawString(x+8,y+32,"Stack Pointer:",0x6688BB);
        const char* hx="0123456789ABCDEF";
        char sb[12]; sb[0]='0';sb[1]='x';
        for(int i=0;i<8;i++) sb[2+i]=hx[(esp_val>>(28-i*4))&0xF]; sb[10]=0;
        DrawString(x+100,y+32,sb,0xFFCC44);
        DrawRect(x+2,y+46,w-4,1,0x204060);
        DrawString(x+8,y+52,"Stack dump (ESP→ESP+0xA0):",0x556677);
        for(int row=0;row<10;row++){
            uint32_t* sp=(uint32_t*)(uintptr_t)(esp_val+(uint32_t)(row*16));
            char ab[12]; ab[0]='0';ab[1]='x';
            uint32_t ra=esp_val+(uint32_t)(row*16);
            for(int i=0;i<8;i++) ab[2+i]=hx[(ra>>(28-i*4))&0xF]; ab[10]=0;
            DrawString(x+4,y+64+row*14,ab,0x4488AA);
            for(int col=0;col<4;col++){
                uint32_t val=*(sp+col);
                char vb[12]; vb[0]='0';vb[1]='x';
                for(int i=0;i<8;i++) vb[2+i]=hx[(val>>(28-i*4))&0xF]; vb[10]=0;
                DrawString(x+80+col*70,y+64+row*14,vb,0x00CC66);
            }
        }
    }
    void OnClickContent(int mx,int my) override { (void)mx;(void)my; }
};

/* ── 7. Kernel Log ── */
struct KernelLog : public Window {
    char lines[20][64]; int line_count;
    KernelLog():Window("Kernel Log",50,30,440,260){ line_count=0;
        AddLine("LightOS 2 Pro kernel initialized");
        AddLine("PIC remapped IRQ0-7 -> 0x20, IRQ8-15 -> 0x28");
        AddLine("SSE2 enabled (CR4.OSFXSR=1)");
        AddLine("MTRR Write-Combining set for framebuffer");
        AddLine("VFS initialized: 128 nodes, 512KB pool");
        AddLine("ACPI: RSDP located, PM1 control ready");
        AddLine("PS/2 mouse initialized (3-byte packets)");
        AddLine("Keyboard: layout=EN-US");
        AddLine("Shell started, compositor online");
        AddLine("[OK] Boot complete");
    }
    void AddLine(const char* s){
        if(line_count>=20) return;
        int i=0; while(s[i]&&i<63){lines[line_count][i]=s[i];i++;}
        lines[line_count][i]=0; line_count++;
    }
    void DrawContent() override {
        DrawRect(x+2,y+26,w-4,h-28,0x060810);
        for(int i=0;i<line_count;i++){
            uint32_t c=0x8899AA;
            if(lines[i][0]=='[') c=0x44FF44;
            DrawString(x+6,y+30+i*12,lines[i],c);
        }
    }
    void OnClickContent(int mx,int my) override { (void)mx;(void)my; }
};

/* ── 8. BSOD Tester ── */
struct BsodTester : public Window {
    int  selected_code;
    bool fake_bsod_active;
    BsodTester():Window("BSOD Tester",120,60,300,240){
        selected_code=0; fake_bsod_active=false;
    }
    void DrawContent() override {
        if(fake_bsod_active){
            /* Sahte BSOD ekranı — pencere içinde */
            DrawRect(x+2,y+26,w-4,h-28,0x0000AA);
            /* Üst ve alt çizgi */
            DrawRect(x+2,y+26,w-4,2,0xFFFFFF);
            DrawRect(x+2,y+h-4,w-4,2,0xFFFFFF);
            /* :( */
            DrawString(x+10,y+36,":(",0xFFFFFF);
            /* Mesaj */
            const char* codes[]={"PAGE_FAULT_IN_NONPAGED_AREA",
                                  "SYSTEM_THREAD_EXCEPTION",
                                  "MEMORY_MANAGEMENT",
                                  "DRIVER_IRQL_NOT_LESS",
                                  "UNEXPECTED_KERNEL_TRAP",
                                  "KERNEL_PANIC"};
            DrawString(x+8,y+56,"Stop code:",0xFFFFFF);
            DrawString(x+8,y+68,codes[selected_code],0xFFFFBB);
            DrawString(x+8,y+86,"Your LightOS ran into",0xDDDDDD);
            DrawString(x+8,y+98,"a problem. (SIMULATED)",0xDDDDDD);
            DrawString(x+8,y+118,"This is a test - system",0xAAAAAA);
            DrawString(x+8,y+130,"is still running.",0xAAAAAA);
            /* ESC veya kapat ile geri */
            DrawRect(x+8,y+h-26,w-16,20,0x000088);
            DrawRectBorder(x+8,y+h-26,w-16,20,0xFFFFFF);
            DrawString(x+w/2-32,y+h-20,"[ESC] Exit Test",0xFFFFFF);
            return;
        }
        DrawRect(x+2,y+26,w-4,h-28,0x080810);
        DrawString(x+8,y+32,"Select BSOD code to trigger:",0x8899BB);
        const char* codes[]={"PAGE_FAULT","STACK_OVERFLOW","MEMORY_MGMT",
                             "DRIVER_IRQL","UNEXPECTED_KERNEL","KERNEL_PANIC"};
        for(int i=0;i<6;i++){
            bool sel=(selected_code==i);
            if(sel) DrawRect(x+4,y+50+i*22,w-8,20,0x100020);
            DrawRectBorder(x+4,y+50+i*22,w-8,20,sel?0xFF0044:0x303048);
            DrawString(x+10,y+55+i*22,codes[i],sel?0xFF8888:0x8888AA);
        }
        DrawGradientV(x+8,y+h-26,w-16,22,0x300010,0x180008);
        DrawBevel(x+8,y+h-26,w-16,22,true);
        DrawString(x+w/2-28,y+h-20,"TRIGGER BSOD",0xFF4444);
    }
    void OnClickContent(int mx,int my) override {
        if(fake_bsod_active){
            /* ESC butonu veya pencere içi tıklama ile çık */
            if(my>=y+h-26) fake_bsod_active=false;
            (void)mx; return;
        }
        for(int i=0;i<6;i++)
            if(my>=y+50+i*22&&my<=y+70+i*22) selected_code=i;
        if(my>=y+h-26&&my<=y+h-4){
            fake_bsod_active=true;
        }
        (void)mx;
    }
    void KeyPress(unsigned int cp){
        if(cp==27) fake_bsod_active=false; /* ESC */
    }
};

/* ── 9. RTC Inspector ── */
struct RtcInspector : public Window {
    RtcInspector():Window("RTC Inspector",90,50,300,200){}
    void DrawContent() override {
        DrawRect(x+2,y+26,w-4,h-28,0x060810);
        RtcTime t=rtc_get();
        DrawString(x+8,y+34,"CMOS RTC Live Readout",0x6688AA);
        DrawRect(x+2,y+46,w-4,1,0x204060);
        char h2[3],m2[3],s2[3],d2[3],mo2[3];
        rtc_2d(t.hour,h2); rtc_2d(t.min,m2); rtc_2d(t.sec,s2);
        rtc_2d(t.day,d2);  rtc_2d(t.month,mo2);
        char ts[12]; ts[0]=h2[0];ts[1]=h2[1];ts[2]=':';ts[3]=m2[0];
        ts[4]=m2[1];ts[5]=':';ts[6]=s2[0];ts[7]=s2[1];ts[8]=0;
        DrawString(x+8,y+54,"Time:",0x556677);
        DrawString(x+56,y+54,ts,0xFFCC44);
        char ds[12]; ds[0]=d2[0];ds[1]=d2[1];ds[2]='.';ds[3]=mo2[0];
        ds[4]=mo2[1];ds[5]='.';
        char yr[5]; int yr2=t.year;
        yr[0]='0'+yr2/1000;yr[1]='0'+(yr2/100)%10;
        yr[2]='0'+(yr2/10)%10;yr[3]='0'+yr2%10;yr[4]=0;
        ds[6]=yr[0];ds[7]=yr[1];ds[8]=yr[2];ds[9]=yr[3];ds[10]=0;
        DrawString(x+8,y+68,"Date:",0x556677);
        DrawString(x+56,y+68,ds,0xFFCC44);
        /* Raw CMOS ports */
        DrawString(x+8,y+88,"Raw CMOS:",0x556677);
        const char* regs[]={"Sec","Min","Hr","Day","Mon","Yr"};
        uint8_t vals[]={t.sec,t.min,t.hour,t.day,t.month,(uint8_t)(t.year%100)};
        const char* hx="0123456789ABCDEF";
        for(int i=0;i<6;i++){
            int rx=x+8+i*44,ry=y+102;
            DrawString(rx,ry,regs[i],0x8899BB);
            char vb[5]; vb[0]='0';vb[1]='x';
            vb[2]=hx[(vals[i]>>4)&0xF];vb[3]=hx[vals[i]&0xF];vb[4]=0;
            DrawString(rx,ry+12,vb,0x00FF88);
        }
    }
    void OnClickContent(int mx,int my) override { (void)mx;(void)my; }
};

/* ── 10. Panic Simulator ── */
struct PanicSim : public Window {
    char msg[64]; int msg_len;
    PanicSim():Window("Panic Simulator",140,70,320,220){
        msg[0]=0; msg_len=0;
    }
    void DrawContent() override {
        DrawRect(x+2,y+26,w-4,h-28,0x080810);
        DrawString(x+8,y+34,"Custom Panic Message:",0x8899BB);
        DrawRect(x+8,y+48,w-16,18,0x0A0A18);
        DrawRectBorder(x+8,y+48,w-16,18,0x304060);
        DrawString(x+12,y+52,msg,0xFFFFFF);
        DrawRect(x+12+msg_len*9,y+50,2,12,0x00AAFF); /* cursor */
        DrawString(x+8,y+80,"Presets:",0x556677);
        const char* presets[]={"Kernel NULL ptr deref",
                               "Stack smashing detected",
                               "Double fault @ 0x0000",
                               "VFS corruption"};
        for(int i=0;i<4;i++){
            DrawGradientV(x+8,y+94+i*24,w-16,20,0x101028,0x080818);
            DrawBevel(x+8,y+94+i*24,w-16,20,true);
            DrawString(x+14,y+99+i*24,presets[i],0x8899CC);
        }
        DrawGradientV(x+8,y+h-26,w-16,22,0x300010,0x180008);
        DrawBevel(x+8,y+h-26,w-16,22,true);
        DrawString(x+w/2-34,y+h-20,"KERNEL PANIC",0xFF4444);
    }
    void OnClickContent(int mx,int my) override {
        const char* presets[]={"Kernel NULL ptr deref",
                               "Stack smashing detected",
                               "Double fault @ 0x0000",
                               "VFS corruption"};
        for(int i=0;i<4;i++)
            if(my>=y+94+i*24&&my<=y+114+i*24){
                msg_len=0; msg[0]=0;
                const char* p=presets[i];
                while(*p&&msg_len<62){msg[msg_len++]=*p++;} msg[msg_len]=0;
            }
        if(my>=y+h-26&&my<=y+h-4)
            BSOD(BSOD_KERNEL_PANIC);
        (void)mx;
    }
    void KeyPress(unsigned int cp){
        if(cp=='\b'){if(msg_len>0){msg[--msg_len]=0;}}
        else if(cp>=' '&&cp<127&&msg_len<62){msg[msg_len++]=(char)cp;msg[msg_len]=0;}
    }
};

#endif
