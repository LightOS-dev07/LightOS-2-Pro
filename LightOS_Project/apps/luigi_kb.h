#ifndef LUIGI_KB_H
#define LUIGI_KB_H
/*
 * apps/luigi_kb.h — LightOS Bilgi Tabanı
 * ==========================================
 * Anahtar kelime örtüşmesine göre konu seçimi.
 * Her konu için birden fazla yanıt varyasyonu —
 * rastgele önek + rastgele şablon kombinasyonu ile
 * aynı soruya hep aynı kelime kelime cevap verilmez.
 */
#include "luigi_lang.h"

/* ── Basit PRNG (gerçek rastgelelik değil ama yeterli çeşitlilik) ── */
static unsigned luigi_rng_state = 7919;
static int luigi_rand(int mod){
    luigi_rng_state = luigi_rng_state*1103515245u + 12345u;
    int v=(int)((luigi_rng_state>>16)&0x7FFF);
    return mod>0 ? v%mod : 0;
}
static void luigi_rng_feed(const char* s){
    while(*s){ luigi_rng_state = luigi_rng_state*131 + (unsigned char)(*s); s++; }
}

static char luigi_lc2(char c){ return (c>='A'&&c<='Z') ? (char)(c-'A'+'a') : c; }
static bool luigi_substr_ci(const char* hay, const char* needle){
    int nl=0; while(needle[nl]) nl++;
    if(nl==0) return false;
    for(int i=0; hay[i]; i++){
        bool m=true;
        for(int j=0;j<nl;j++){
            if(!hay[i+j]){ m=false; break; }
            if(luigi_lc2(hay[i+j])!=luigi_lc2(needle[j])){ m=false; break; }
        }
        if(m) return true;
    }
    return false;
}

struct LuigiKBEntry {
    const char* kw_tr[7];
    const char* kw_en[7];
    const char* resp_tr[3];
    const char* resp_en[3];
};

static const LuigiKBEntry LUIGI_KB[] = {
/* GREETING */
{ {"selam","merhaba","naber","gunaydin","iyi aksamlar",0,0},
  {"hello","hi","hey","good morning","good evening",0,0},
  { "Selam! Ben Luigi, LightOS'un icindeki yapay zeka asistaniyim. Matematik sorabilir, sistem hakkinda soru sorabilir ya da sohbet edebiliriz.",
    "Merhaba! Nasil yardimci olabilirim? Hesap yapabilirim, LightOS'u anlatabilirim, ya da konusabiliriz.", 0 },
  { "Hey there! I'm Luigi, the assistant living inside LightOS. I can do math, answer questions about the system, or just chat.",
    "Hi! What can I help with? I can calculate things, explain LightOS features, or we can just talk.", 0 } },

/* THANKS */
{ {"tesekkur","sagol","saol","eyvallah",0,0,0},
  {"thanks","thank you","appreciate","cheers",0,0,0},
  { "Rica ederim! Baska bir sorun olursa buradayim.",
    "Ne demek, yardimci olabildiysem ne mutlu.", 0 },
  { "You're welcome! Let me know if you need anything else.",
    "No problem at all — happy to help.", 0 } },

/* IDENTITY */
{ {"kimsin","sensin","adin ne","sen nesin","ai misin",0,0},
  {"who are you","what are you","your name","are you ai",0,0,0},
  { "Ben Luigi — LightOS 2 Pro'nun icine gomulu kucuk bir asistanim. Gercek bir bulut tabanli yapay zeka degilim; matematik motoru, dil algilama ve bir bilgi tabaniyla calisiyorum. Yine de elimden geleni yapariim!",
    "Adim Luigi. LightOS icinde yasayan, hesap yapabilen ve sistem hakkinda bilgi verebilen bir yardimciyim.", 0 },
  { "I'm Luigi — a small assistant built directly into LightOS 2 Pro. I'm not a cloud-based LLM; I run on a math engine, language detection, and a knowledge base. But I do my best to be genuinely useful!",
    "My name is Luigi. I live inside LightOS and can do math, answer system questions, and chat.", 0 } },

/* CAPABILITIES */
{ {"ne yapabilirsin","yardim et","ne biliyorsun","yetenekler",0,0,0},
  {"what can you do","help me","capabilities","what do you know",0,0,0},
  { "Su anda yapabildiklerim: matematik ifadeleri cozme (ornek: sqrt(144)+5), LightOS uygulamalari ve kisayollari hakkinda bilgi verme, kullanim kilavuzu olusturma, ve sohbet etme. Sor bakalim!",
    "Hesap makinesi gibi calisirim, LightOS'un her kosesini bilirim, ve istersen bir kullanim kilavuzu da yazabilirim. Ne ile basliyoruz?", 0 },
  { "Right now I can: solve math expressions (e.g. sqrt(144)+5), answer questions about LightOS apps and shortcuts, generate a usage guide, and chat. Try me!",
    "I work like a calculator, I know every corner of LightOS, and I can write you a usage guide if you'd like. Where should we start?", 0 } },

/* APPS LIST */
{ {"uygulamalar","programlar","neler var","app listesi",0,0,0},
  {"applications","apps list","what apps","programs",0,0,0},
  { "LightOS'ta su uygulamalar var: Not Defteri, Hesap Makinesi, Dosyalar, Ayarlar, Terminal, Paint, Saat, Sistem Bilgisi, LightSurf tarayici, Takvim, Gorev Yoneticisi, ve XaefGAMES oyun klasoru.",
    "Masaustunde 12 uygulama bulunur: temel araclar (Not Defteri, Hesap Makinesi, Dosyalar) ve daha gelismis olanlar (Terminal, LightSurf, Takvim, Gorev Yoneticisi, oyunlar).", 0 },
  { "LightOS includes: Notepad, Calculator, Files, Settings, Terminal, Paint, Clock, System Info, the LightSurf browser, Calendar, Task Manager, and the XaefGAMES folder.",
    "There are 12 apps on the desktop: basics like Notepad and Calculator, plus more advanced tools like Terminal, LightSurf, Calendar, Task Manager, and games.", 0 } },

/* NOTEPAD */
{ {"not defteri","notepad","yazi yaz",0,0,0,0},
  {"notepad","text editor","write text",0,0,0,0},
  { "Not Defteri basit bir metin editoru. Ctrl+S ile dosyani VFS'e kaydedebilirsin, Dosyalar uygulamasindan tekrar acabilirsin.",
    "Not Defteri ile yazi yazip Ctrl+S'le kaydedersin. Baslikta yildiz (*) varsa kaydedilmemis degisiklik var demektir.", 0 },
  { "Notepad is a simple text editor. Press Ctrl+S to save your file to the VFS, then reopen it anytime from File Manager.",
    "Use Notepad to write text and Ctrl+S to save. An asterisk (*) in the title means unsaved changes.", 0 } },

/* CALCULATOR */
{ {"hesap makinesi","calculator app",0,0,0,0,0},
  {"calculator app","calc app",0,0,0,0,0},
  { "Hesap Makinesi uygulamasi temel +,-,x,/ islemlerini yapar. Ama bana da direkt matematik sorabilirsin, mesela '12*7' yaz, hesaplarim!",
    "Masaustundeki Hesap Makinesi basit aritmetik icin. Istersen bana da yazabilirsin, ben de cozerim.", 0 },
  { "The Calculator app handles basic +,-,x,/ operations. But you can also just ask me directly — try typing '12*7' and I'll solve it!",
    "The desktop Calculator does simple arithmetic. Or just type the expression to me and I'll compute it.", 0 } },

/* FILES */
{ {"dosyalar","dosya yoneticisi","klasor","vfs",0,0,0},
  {"files","file manager","folder","explorer",0,0,0},
  { "Dosyalar uygulamasi VFS (sanal dosya sistemi) icinde gezinmeni saglar. Cift tikla klasore gir, Backspace ile geri don, R ile yeniden adlandir.",
    "LightOS'ta dosyalar RAM tabanli bir VFS'te tutulur — yeniden baslatinca sifirlanir. Maksimum 128 dugum, dosya basina 4KB.", 0 },
  { "File Manager lets you browse the VFS (virtual file system). Double-click to enter folders, Backspace to go up, R to rename.",
    "LightOS files live in a RAM-based VFS — it resets on reboot. Max 128 nodes, 4KB per file.", 0 } },

/* SETTINGS */
{ {"ayarlar","tema","duvar kagidi","klavye duzeni",0,0,0},
  {"settings","theme","wallpaper","keyboard layout",0,0,0},
  { "Ayarlar'da 3 sekme var: Klavye (TR-Q / EN-US), Tema (Luna Steel / ColdIceBar), ve Duvar Kagidi (Luna Night / Dawn / Forest).",
    "Temani ve duvar kagidini Ayarlar'dan degistirebilirsin. ColdIceBar buzlu cam efekti verir, Luna Steel klasik gorunum.", 0 },
  { "Settings has 3 tabs: Keyboard (TR-Q / EN-US), Theme (Luna Steel / ColdIceBar), and Wallpaper (Luna Night / Dawn / Forest).",
    "You can change theme and wallpaper from Settings. ColdIceBar gives a frosted-glass look, Luna Steel is the classic style.", 0 } },

/* TERMINAL */
{ {"terminal","komut satiri","ls","cd komutu",0,0,0},
  {"terminal","command line","ls command","cd command",0,0,0},
  { "Terminal'de ls, cd, cat, echo, mkdir, rm, pwd, clear, ver, mem, help komutlarini kullanabilirsin. Ok tuslariyla komut gecmisinde gezebilirsin.",
    "Terminal tam bir VFS komut satiri. 'help' yazarsan tum komutlari listeler.", 0 },
  { "In Terminal you can use ls, cd, cat, echo, mkdir, rm, pwd, clear, ver, mem, and help. Arrow keys browse command history.",
    "Terminal is a full VFS command line. Type 'help' to list every command.", 0 } },

/* PAINT */
{ {"paint","resim yap","cizim","boya",0,0,0},
  {"paint","drawing","draw","painting app",0,0,0},
  { "Paint ile piksel sanati yapabilirsin: Kalem, Silgi, Doldur ve Cizgi araclari var. 16 renk paleti ve ayarlanabilir firca boyutu mevcut.",
    "320x200 piksel tuvalde cizim yapabilirsin. Arac kutusunda Pen/Eraser/Fill/Line secenekleri var.", 0 },
  { "Paint lets you create pixel art with Pen, Eraser, Fill, and Line tools. There's a 16-color palette and adjustable brush size.",
    "You get a 320x200 pixel canvas. The toolbox has Pen/Eraser/Fill/Line options.", 0 } },

/* CLOCK / SYSINFO */
{ {"saat","sistem bilgisi","ram","bellek",0,0,0},
  {"clock","system info","ram amount","memory",0,0,0},
  { "Saat uygulamasi gercek RTC zamanini analog ve dijital olarak gosterir. Sistem Bilgisi'nde ise RAM, ekran cozunurlugu, ve VFS durumu var.",
    "Sistem Bilgisi gercek zamanli RAM kullanimini ve dev mode durumunu gosterir.", 0 },
  { "The Clock app shows real RTC time both as an analog face and digitally. System Info shows RAM, resolution, and VFS status.",
    "System Info displays live RAM usage and dev mode status.", 0 } },

/* BROWSER */
{ {"tarayici","lightsurf","internet","http",0,0,0},
  {"browser","lightsurf","internet","website",0,0,0},
  { "LightSurf, LightOS'un kendi tarayicisi. about: sayfalarini ve file:// VFS dosyalarini gosterebilir. Ctrl+F sayfada arar, Ctrl+D favorilere ekler.",
    "Gercek internet baglantisi icin RTL8139 veya Intel e1000 NIC gerekir. Su an HTTP destegi gelistirme asamasinda.", 0 },
  { "LightSurf is LightOS's built-in browser. It can show about: pages and file:// VFS content. Ctrl+F searches the page, Ctrl+D bookmarks it.",
    "Real internet access needs an RTL8139 or Intel e1000 NIC. HTTP support is still in development.", 0 } },

/* CALENDAR */
{ {"takvim","ay gorunumu","not ekle",0,0,0,0},
  {"calendar","month view","add note",0,0,0,0},
  { "Takvim gercek RTC tarihini gosterir, aylar arasinda gezinebilir ve gunlere not ekleyebilirsin — notlar VFS'e kaydedilir.",
    "Takvim'de bir gune tiklayip not yazabilir, Save'e basarak Calendar klasorune kaydedebilirsin.", 0 },
  { "Calendar shows the real RTC date, lets you navigate months, and add notes per day — notes get saved to the VFS.",
    "Click a day in Calendar, type a note, and hit Save to store it in the Calendar folder.", 0 } },

/* TASK MANAGER */
{ {"gorev yoneticisi","taskman","pencereler","bellek kullanimi",0,0,0},
  {"task manager","windows list","memory usage",0,0,0,0},
  { "Gorev Yoneticisi acik tum pencereleri, durumlarini (Active/Minimized/Crashed) ve yaklasik bellek kullanimini gosterir. End Task ile bir pencereyi kapatabilirsin.",
    "Cokmuş bir uygulama varsa Gorev Yoneticisi'nde kirmizi 'CRASHED' etiketiyle gorunur.", 0 },
  { "Task Manager lists every open window with its status (Active/Minimized/Crashed) and approximate memory use. Use End Task to close one.",
    "If an app crashed, you'll see it tagged red as 'CRASHED' in Task Manager.", 0 } },

/* GAMES GENERAL */
{ {"oyun","oyunlar","xaefgames","oyna",0,0,0},
  {"game","games","xaefgames","play",0,0,0},
  { "XaefGAMES klasorunde 4 oyun var: Snake, Tetris, Pong, ve Minesweeper. Masaustundeki klasore tikla, sonra istedigin oyuna tikla.",
    "Oyunlar gercek zamanli calisir — PIT donanim sayaciyla senkronize, yani CPU hizindan bagimsiz akar.", 0 },
  { "The XaefGAMES folder has 4 games: Snake, Tetris, Pong, and Minesweeper. Click the desktop folder, then click any game.",
    "Games run in real time, synced to the hardware PIT counter — so speed stays consistent regardless of CPU speed.", 0 } },

/* SNAKE */
{ {"yilan oyunu","snake nasil",0,0,0,0,0},
  {"snake game","how to play snake",0,0,0,0,0},
  { "Snake'de WASD ya da ok tuslariyla yon verirsin, SPACE ile baslar, R ile yeniden baslatirsin. Yedikce hizlanir!",
    "Snake klasik bir yilan oyunu — kendine ya da duvara carparsan oyun biter.", 0 },
  { "In Snake, use WASD or arrow keys to steer, SPACE to start, R to restart. It speeds up as you eat!",
    "Snake is the classic snake game — hitting yourself or the wall ends it.", 0 } },

/* TETRIS */
{ {"tetris nasil","blok oyunu",0,0,0,0,0},
  {"tetris game","how to play tetris",0,0,0,0,0},
  { "Tetris'te A/D ile yatay hareket, S ile hizli dusur, W ile dondur. SPACE baslatir, R yeniden baslatir. Satir doldurunca puan kazanirsin.",
    "Seviye yukseldikce parcalar daha hizli duser.", 0 },
  { "In Tetris, A/D move horizontally, S soft-drops, W rotates. SPACE starts, R restarts. Clear lines to score.",
    "Pieces fall faster as your level increases.", 0 } },

/* PONG */
{ {"pong nasil","ping pong oyunu",0,0,0,0,0},
  {"pong game","how to play pong",0,0,0,0,0},
  { "Pong'da W/S ile paddle'ini hareket ettirirsin, SPACE ile baslar. Rakip bilgisayar kontrollu (basit AI).",
    "Pong klasik bir masa tenisi simulasyonu — top kacirilirsa rakip puan alir.", 0 },
  { "In Pong, W/S move your paddle, SPACE starts the match. The opponent is a simple AI.",
    "Pong is the classic table-tennis simulation — missing the ball scores the other side.", 0 } },

/* MINESWEEPER */
{ {"mayin tarlasi","mines nasil",0,0,0,0,0},
  {"minesweeper game","how to play minesweeper",0,0,0,0,0},
  { "Mayin Tarlasi'nda hucrelere tiklayip mayinlardan kacinirsin. Sayilar etrafdaki mayin sayisini gosterir. Ustteki yuz butonuyla yeniden baslarsin.",
    "16x12 bir tahta ve 30 mayin var — klasik zorluk seviyesi.", 0 },
  { "In Minesweeper, click cells to avoid mines. Numbers show how many mines surround that cell. Click the face button to restart.",
    "It's a 16x12 board with 30 mines — classic difficulty.", 0 } },

/* DEV MODE */
{ {"developer mode","gelistirici modu","f4","dev tools",0,0,0},
  {"developer mode","dev mode","f4 key","dev tools",0,0,0},
  { "Gelistirici Modu'nu acmak icin acilis animasyonu sirasinda F4 tusuna 5 kere bas. Onay sorusu cikar, Y ile onaylarsin. 10 dev tool acilir: Mem Viewer, Port I/O, VFS Inspector, ve daha fazlasi.",
    "Dev Mode'da masaustunde 10 ekstra arac ikonu belirir ve Dosyalar'da sistem dosyalari gorunur hale gelir.", 0 },
  { "To enable Developer Mode, press F4 five times during the boot animation. A confirmation appears — press Y. This unlocks 10 dev tools: Mem Viewer, Port I/O, VFS Inspector, and more.",
    "In Dev Mode, 10 extra tool icons appear on the desktop and system files become visible in File Manager.", 0 } },

/* SHORTCUTS */
{ {"kisayol","kisayollar","klavye kisayolu",0,0,0,0},
  {"shortcut","shortcuts","keyboard shortcut",0,0,0,0},
  { "Onemli kisayollar: Ctrl+S kaydet (Not Defteri), Ctrl+F sayfada ara (LightSurf), Ctrl+D favori ekle (LightSurf), R yeniden adlandir (Dosyalar), F4x5 Dev Mode.",
    "ESC arama/duzenlemeyi kapatir, ok tuslari gecmiste gezinir (Terminal) veya kaydirir (LightSurf).", 0 },
  { "Key shortcuts: Ctrl+S save (Notepad), Ctrl+F search page (LightSurf), Ctrl+D bookmark (LightSurf), R rename (File Manager), F4x5 for Dev Mode.",
    "ESC closes search/editing, arrow keys browse history (Terminal) or scroll (LightSurf).", 0 } },

/* NETWORK */
{ {"ag","network","internet baglantisi","nic","dhcp",0,0},
  {"network","internet connection","nic","dhcp",0,0,0},
  { "LightOS, RTL8139 ve Intel e1000 ag kartlarini destekler. Acilista DHCP ile otomatik IP alir. QEMU'da -device e1000 ile test edebilirsin.",
    "Ag yigini Ethernet, ARP, IP, TCP, DHCP ve DNS katmanlarini icerir — sifirdan yazilmis.", 0 },
  { "LightOS supports RTL8139 and Intel e1000 network cards. It auto-acquires an IP via DHCP at boot. Test it in QEMU with -device e1000.",
    "The network stack includes Ethernet, ARP, IP, TCP, DHCP, and DNS layers — all written from scratch.", 0 } },

/* ABOUT OS / AUTHOR */
{ {"lightos nedir","kim yapti","yazar","gelistirici",0,0,0},
  {"what is lightos","who made it","author","developer",0,0,0},
  { "LightOS 2 Pro, Xaef BTL tarafindan 2026'da gelistirilen, sifirdan yazilmis 64-bit x86 bare-metal bir isletim sistemi. Linux ya da herhangi bir mevcut cekirdek kullanmiyor.",
    "Bu tamamen ozel bir isletim sistemi — kendi GUI'si, kendi dosya sistemi, kendi ag yigini var.", 0 },
  { "LightOS 2 Pro is a from-scratch 64-bit x86 bare-metal operating system, developed by Xaef BTL in 2026. It doesn't use Linux or any existing kernel.",
    "This is a fully custom OS — its own GUI, its own filesystem, its own network stack.", 0 } },

/* GUIDE REQUEST (trigger handled separately, but keep a description here too) */
{ {"kilavuz","rehber","kullanim kilavuzu",0,0,0,0},
  {"guide","manual","usage guide",0,0,0,0},
  { "Senin icin bir kullanim kilavuzu hazirlayip VFS'e Guide.txt olarak kaydedebilirim. Sadece 'kilavuz olustur' yaz.",
    "Kilavuz istersen, Dosyalar'dan acabilecegin Guide.txt dosyasini olustururum.", 0 },
  { "I can prepare a usage guide for you and save it to the VFS as Guide.txt. Just say 'create guide'.",
    "If you want a guide, I'll generate a Guide.txt file you can open from File Manager.", 0 } },
};
static const int LUIGI_KB_COUNT = (int)(sizeof(LUIGI_KB)/sizeof(LUIGI_KB[0]));

/* Sohbet onekleri — yanitin basina rastgele eklenir (bos da olabilir) */
static const char* LUIGI_PREFIX_TR[] = {
    "", "Tabii, ", "Bakalim: ", "Iste: ", "Soyle soyleyeyim: "
};
static const char* LUIGI_PREFIX_EN[] = {
    "", "Sure — ", "Let's see: ", "Here: ", "So, "
};

/* En iyi eşleşen konuyu bul, skor 0 ise -1 döner */
static int luigi_match_topic(const char* msg, LuigiLang lang){
    int best=-1, best_score=0;
    for(int i=0;i<LUIGI_KB_COUNT;i++){
        const char* const* kws = (lang==LUIGI_TR) ? LUIGI_KB[i].kw_tr : LUIGI_KB[i].kw_en;
        int score=0;
        for(int k=0;k<7 && kws[k];k++)
            if(luigi_substr_ci(msg, kws[k])) score++;
        if(score>best_score){ best_score=score; best=i; }
    }
    return best;
}

/* Konu icin rastgele bir yanit varyasyonu sec, onek ekle */
static void luigi_build_kb_response(int topic, LuigiLang lang, char* out, int outmax){
    const char* const* resp = (lang==LUIGI_TR) ? LUIGI_KB[topic].resp_tr : LUIGI_KB[topic].resp_en;
    int n=0; while(n<3 && resp[n]) n++;
    int pick = n>0 ? luigi_rand(n) : 0;
    const char* body = (n>0) ? resp[pick] : "";

    const char* const* prefixes = (lang==LUIGI_TR) ? LUIGI_PREFIX_TR : LUIGI_PREFIX_EN;
    int pcount = 5;
    const char* pre = prefixes[luigi_rand(pcount)];

    int oi=0;
    while(*pre && oi<outmax-1) out[oi++]=*pre++;
    while(*body && oi<outmax-1) out[oi++]=*body++;
    out[oi]=0;
}
#endif
